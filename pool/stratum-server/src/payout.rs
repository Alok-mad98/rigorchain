//! In-memory ORU-only PPLNS payout ledger (TASK-MINING-009 §6).
//!
//! REORG-01 (TASK-RELEASE-006-M11): FINALIZED → REVERSED on deep reorg with
//! deterministic clawback of `oru_pending` and share orphaning.
//! Consensus impact: NONE.
//!
//! SECURITY-001-E:
//! - **C-6a**: finalizing a round now *consumes* its share window
//!   (`settled_round`). Previously every `ShareCredit` stayed active forever, so
//!   the same shares were re-paid on every subsequent block and pool liabilities
//!   diverged linearly with block count.
//! - **H-5e**: payout splits are computed in integer arithmetic (`u128`) with a
//!   deterministic largest-remainder distribution, so `sum(lines) == coinbase`
//!   exactly. `f64` money math silently lost/created atoms.

use std::collections::HashMap;
use std::sync::{Arc, RwLock};

use block_assembler::JobId;
use sha2::{Digest, Sha256};

/// Fixed-point scale for share difficulty weights (6 decimal places).
///
/// Stratum difficulty is fractional by protocol, but money must be split with
/// integers. Difficulty is quantised once, at credit time, and every downstream
/// computation is exact integer arithmetic.
pub const DIFFICULTY_SCALE: u64 = 1_000_000;

/// Quantise a Stratum difficulty into integer weight units.
pub fn difficulty_to_weight(difficulty: f64) -> u64 {
    if !difficulty.is_finite() || difficulty <= 0.0 {
        return 0;
    }
    let scaled = difficulty * DIFFICULTY_SCALE as f64;
    if scaled >= u64::MAX as f64 {
        return u64::MAX;
    }
    // Round-half-up, then clamp so a positive difficulty never becomes zero weight.
    ((scaled + 0.5) as u64).max(1)
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct MinerBalance {
    pub worker: String,
    pub oru_pending: i64,
    pub shares_credited: u64,
    pub shares_orphaned: u64,
}

#[derive(Clone, Debug)]
pub struct ShareCredit {
    pub share_id: String,
    pub worker: String,
    pub job_id: JobId,
    /// Integer weight units (`difficulty * DIFFICULTY_SCALE`).
    pub weight: u64,
    pub orphaned: bool,
    /// `Some(round_seq)` once this share has been paid out. Settled shares leave
    /// the active PPLNS window and can never be paid again (C-6a).
    pub settled_round: Option<u64>,
}

impl ShareCredit {
    /// Difficulty as originally credited (derived from the integer weight).
    pub fn difficulty(&self) -> f64 {
        self.weight as f64 / DIFFICULTY_SCALE as f64
    }

    /// Share is inside the active PPLNS window.
    pub fn is_active(&self) -> bool {
        !self.orphaned && self.settled_round.is_none()
    }
}

/// Payout round lifecycle (POSTGRESQL_PERSISTENCE_ARCHITECTURE §7.3 / REORG-01).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum PayoutRoundStatus {
    Open = 0,
    Finalized = 1,
    Orphaned = 2,
    Reversed = 3,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PayoutLine {
    pub worker: String,
    pub amount_oru: i64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PayoutRound {
    pub round_seq: u64,
    pub job_id: JobId,
    pub coinbase_oru: i64,
    pub status: PayoutRoundStatus,
    pub lines: Vec<PayoutLine>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ReverseReport {
    pub rounds_reversed: u64,
    pub shares_orphaned: u64,
    pub oru_clawed_back: i64,
    pub reversed_round_seqs: Vec<u64>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PayoutLedgerSnapshot {
    pub round_id: u64,
    pub blocks_found: u64,
    pub blocks_orphaned: u64,
    pub rounds: Vec<PayoutRound>,
    pub balances: Vec<MinerBalance>,
    pub active_share_count: u64,
    pub orphaned_share_count: u64,
    /// Shares consumed by a finalized round (C-6a).
    pub settled_share_count: u64,
}

pub struct PayoutEngine {
    inner: RwLock<PayoutInner>,
    pub round_id: u64,
}

struct PayoutInner {
    shares: Vec<ShareCredit>,
    balances: HashMap<String, MinerBalance>,
    rounds: Vec<PayoutRound>,
    next_round_seq: u64,
    blocks_found: u64,
    blocks_orphaned: u64,
}

impl PayoutEngine {
    pub fn new(round_id: u64) -> Self {
        Self {
            inner: RwLock::new(PayoutInner {
                shares: Vec::new(),
                balances: HashMap::new(),
                rounds: Vec::new(),
                next_round_seq: 1,
                blocks_found: 0,
                blocks_orphaned: 0,
            }),
            round_id,
        }
    }

    pub fn credit_share(&self, worker: &str, job_id: &JobId, share_id: &str, difficulty: f64) {
        self.credit_share_weighted(worker, job_id, share_id, difficulty_to_weight(difficulty));
    }

    /// Credit a share using integer weight units directly (H-5e).
    pub fn credit_share_weighted(
        &self,
        worker: &str,
        job_id: &JobId,
        share_id: &str,
        weight: u64,
    ) {
        if let Ok(mut inner) = self.inner.write() {
            inner.shares.push(ShareCredit {
                share_id: share_id.to_string(),
                worker: worker.to_string(),
                job_id: job_id.clone(),
                weight,
                orphaned: false,
                settled_round: None,
            });
            let bal = inner
                .balances
                .entry(worker.to_string())
                .or_insert_with(|| MinerBalance {
                    worker: worker.to_string(),
                    ..Default::default()
                });
            bal.shares_credited += 1;
        }
    }

    /// Legacy API retained for unit fixtures only.
    ///
    /// C-6e: this binds the round to a synthetic `anon-<round_id>` job id, which
    /// can never match a real `JobId`, so a later reorg could not roll the round
    /// back. Production callers MUST use [`Self::record_block_found_for_job`] with
    /// the real job id.
    #[deprecated(
        note = "C-6e: use record_block_found_for_job with the real JobId so REORG-01 rollback can match"
    )]
    pub fn record_block_found(&self, coinbase_oru: i64) {
        self.record_block_found_for_job(&JobId(format!("anon-{}", self.round_id)), coinbase_oru);
    }

    /// Finalize a PPLNS round for a found block / job (status=FINALIZED).
    ///
    /// C-6a: the shares that funded this round are marked settled and leave the
    /// active window, so a subsequent block cannot pay them a second time.
    pub fn record_block_found_for_job(&self, job_id: &JobId, coinbase_oru: i64) -> u64 {
        let mut seq = 0u64;
        if let Ok(mut inner) = self.inner.write() {
            inner.blocks_found += 1;
            seq = inner.next_round_seq;
            inner.next_round_seq += 1;

            let lines = Self::compute_pplns_lines(&inner, coinbase_oru);
            for line in &lines {
                let bal = inner
                    .balances
                    .entry(line.worker.clone())
                    .or_insert_with(|| MinerBalance {
                        worker: line.worker.clone(),
                        ..Default::default()
                    });
                bal.oru_pending += line.amount_oru;
            }
            // C-6a: consume the window that was just paid.
            for share in &mut inner.shares {
                if share.is_active() {
                    share.settled_round = Some(seq);
                }
            }
            inner.rounds.push(PayoutRound {
                round_seq: seq,
                job_id: job_id.clone(),
                coinbase_oru,
                status: PayoutRoundStatus::Finalized,
                lines,
            });
        }
        seq
    }

    /// Orphan unpaid shares for jobs (pre-finalization / soft reorg path).
    pub fn orphan_round_for_jobs(&self, job_ids: &[JobId]) {
        if let Ok(mut inner) = self.inner.write() {
            inner.blocks_orphaned += 1;
            let mut workers_orphaned: Vec<String> = Vec::new();
            for share in &mut inner.shares {
                if job_ids.iter().any(|j| j == &share.job_id) && !share.orphaned {
                    share.orphaned = true;
                    workers_orphaned.push(share.worker.clone());
                }
            }
            for worker in workers_orphaned {
                if let Some(bal) = inner.balances.get_mut(&worker) {
                    bal.shares_orphaned += 1;
                }
            }
            // Mark OPEN rounds orphaned (none expected currently; FINALIZED handled by REORG-01).
            for round in &mut inner.rounds {
                if job_ids.iter().any(|j| j == &round.job_id)
                    && round.status == PayoutRoundStatus::Open
                {
                    round.status = PayoutRoundStatus::Orphaned;
                }
            }
        }
    }

    /// REORG-01: reverse FINALIZED payout rounds for orphaned jobs (deep reorg clawback).
    pub fn reverse_finalized_for_jobs(&self, job_ids: &[JobId]) -> ReverseReport {
        let mut report = ReverseReport::default();
        if let Ok(mut inner) = self.inner.write() {
            let mut clawbacks: Vec<(String, i64)> = Vec::new();
            for round in &mut inner.rounds {
                if round.status != PayoutRoundStatus::Finalized {
                    continue;
                }
                if !job_ids.iter().any(|j| j == &round.job_id) {
                    continue;
                }
                for line in &round.lines {
                    clawbacks.push((line.worker.clone(), line.amount_oru));
                    report.oru_clawed_back += line.amount_oru;
                }
                round.status = PayoutRoundStatus::Reversed;
                report.rounds_reversed += 1;
                report.reversed_round_seqs.push(round.round_seq);
            }
            for (worker, amount) in clawbacks {
                if let Some(bal) = inner.balances.get_mut(&worker) {
                    bal.oru_pending -= amount;
                }
            }
            // Orphan shares tied to those jobs (idempotent).
            let mut newly_orphaned_workers: Vec<String> = Vec::new();
            for share in &mut inner.shares {
                if job_ids.iter().any(|j| j == &share.job_id) && !share.orphaned {
                    share.orphaned = true;
                    report.shares_orphaned += 1;
                    newly_orphaned_workers.push(share.worker.clone());
                }
            }
            for worker in newly_orphaned_workers {
                if let Some(bal) = inner.balances.get_mut(&worker) {
                    bal.shares_orphaned += 1;
                }
            }
            if report.rounds_reversed > 0 {
                inner.blocks_orphaned += report.rounds_reversed;
            }
        }
        report
    }

    pub fn balance(&self, worker: &str) -> Option<MinerBalance> {
        self.inner.read().ok()?.balances.get(worker).cloned()
    }

    pub fn total_shares(&self) -> u64 {
        self.inner
            .read()
            .map(|i| i.shares.len() as u64)
            .unwrap_or(0)
    }

    pub fn rounds(&self) -> Vec<PayoutRound> {
        self.inner
            .read()
            .map(|i| i.rounds.clone())
            .unwrap_or_default()
    }

    pub fn snapshot(&self) -> PayoutLedgerSnapshot {
        let inner = self.inner.read().expect("payout lock");
        let mut balances: Vec<MinerBalance> = inner.balances.values().cloned().collect();
        balances.sort_by(|a, b| a.worker.cmp(&b.worker));
        let active_share_count = inner.shares.iter().filter(|s| s.is_active()).count() as u64;
        let orphaned_share_count = inner.shares.iter().filter(|s| s.orphaned).count() as u64;
        let settled_share_count = inner
            .shares
            .iter()
            .filter(|s| s.settled_round.is_some())
            .count() as u64;
        PayoutLedgerSnapshot {
            round_id: self.round_id,
            blocks_found: inner.blocks_found,
            blocks_orphaned: inner.blocks_orphaned,
            rounds: inner.rounds.clone(),
            balances,
            active_share_count,
            orphaned_share_count,
            settled_share_count,
        }
    }

    /// Number of shares currently inside the active PPLNS window (C-6a probe).
    pub fn active_share_count(&self) -> u64 {
        self.inner
            .read()
            .map(|i| i.shares.iter().filter(|s| s.is_active()).count() as u64)
            .unwrap_or(0)
    }

    /// Total ORU booked as owed to miners across all balances.
    pub fn total_liability_oru(&self) -> i64 {
        self.inner
            .read()
            .map(|i| i.balances.values().map(|b| b.oru_pending).sum())
            .unwrap_or(0)
    }

    /// Canonical SHA-256 over snapshot fields for deterministic recovery checks.
    pub fn ledger_state_hash(&self) -> String {
        let snap = self.snapshot();
        let mut h = Sha256::new();
        h.update(snap.round_id.to_le_bytes());
        h.update(snap.blocks_found.to_le_bytes());
        h.update(snap.blocks_orphaned.to_le_bytes());
        h.update(snap.active_share_count.to_le_bytes());
        h.update(snap.orphaned_share_count.to_le_bytes());
        h.update(snap.settled_share_count.to_le_bytes());
        for r in &snap.rounds {
            h.update(r.round_seq.to_le_bytes());
            h.update(r.job_id.0.as_bytes());
            h.update(r.coinbase_oru.to_le_bytes());
            h.update([r.status as u8]);
            for line in &r.lines {
                h.update(line.worker.as_bytes());
                h.update(line.amount_oru.to_le_bytes());
            }
        }
        for b in &snap.balances {
            h.update(b.worker.as_bytes());
            h.update(b.oru_pending.to_le_bytes());
            h.update(b.shares_credited.to_le_bytes());
            h.update(b.shares_orphaned.to_le_bytes());
        }
        hex::encode(h.finalize())
    }

    /// Split `coinbase_oru` across the active PPLNS window.
    ///
    /// H-5e: exact integer arithmetic. Floor shares first, then hand the remainder
    /// out one atom at a time by descending remainder (ties broken by worker name)
    /// so that `sum(lines) == coinbase_oru` exactly and the result is deterministic
    /// across replicas.
    fn compute_pplns_lines(inner: &PayoutInner, coinbase_oru: i64) -> Vec<PayoutLine> {
        if coinbase_oru <= 0 {
            return Vec::new();
        }
        let mut by_worker: HashMap<String, u128> = HashMap::new();
        let mut total_weight: u128 = 0;
        for share in inner.shares.iter().filter(|s| s.is_active()) {
            if share.weight == 0 {
                continue;
            }
            *by_worker.entry(share.worker.clone()).or_default() += share.weight as u128;
            total_weight += share.weight as u128;
        }
        if total_weight == 0 {
            return Vec::new();
        }

        let pot = coinbase_oru as u128;
        let mut workers: Vec<String> = by_worker.keys().cloned().collect();
        workers.sort();

        let mut lines: Vec<PayoutLine> = Vec::with_capacity(workers.len());
        let mut remainders: Vec<(u128, usize)> = Vec::with_capacity(workers.len());
        let mut assigned: u128 = 0;
        for (idx, w) in workers.iter().enumerate() {
            let weight = by_worker[w];
            let numerator = pot * weight;
            let floor = numerator / total_weight;
            assigned += floor;
            remainders.push((numerator % total_weight, idx));
            lines.push(PayoutLine {
                worker: w.clone(),
                amount_oru: floor as i64,
            });
        }

        // Descending remainder, ascending index (worker name order) for ties.
        remainders.sort_by(|a, b| b.0.cmp(&a.0).then(a.1.cmp(&b.1)));
        let mut leftover = pot - assigned;
        for (_, idx) in remainders {
            if leftover == 0 {
                break;
            }
            lines[idx].amount_oru += 1;
            leftover -= 1;
        }
        debug_assert_eq!(
            lines.iter().map(|l| l.amount_oru as i128).sum::<i128>(),
            coinbase_oru as i128
        );
        lines
    }
}

pub type SharedPayoutEngine = Arc<PayoutEngine>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pplns_distributes_oru_on_block_found() {
        let engine = PayoutEngine::new(1);
        let job = JobId("j1".into());
        engine.credit_share("alice", &job, "s1", 10.0);
        engine.credit_share("bob", &job, "s2", 30.0);
        engine.record_block_found_for_job(&job, 1_000_000);
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 250_000);
        assert_eq!(engine.balance("bob").unwrap().oru_pending, 750_000);
    }

    /// C-6a regression: the share window must be consumed on finalize. Before the
    /// fix, a second block re-paid the same shares and liabilities grew linearly
    /// with block count for zero additional work.
    #[test]
    fn c6a_share_window_is_consumed_and_never_paid_twice() {
        let engine = PayoutEngine::new(1);
        let job1 = JobId("j1".into());
        engine.credit_share("alice", &job1, "s1", 10.0);
        engine.credit_share("bob", &job1, "s2", 30.0);
        assert_eq!(engine.active_share_count(), 2);

        engine.record_block_found_for_job(&job1, 1_000_000);
        assert_eq!(engine.active_share_count(), 0);
        assert_eq!(engine.total_liability_oru(), 1_000_000);

        // Second block with no new shares must pay nothing at all.
        let seq2 = engine.record_block_found_for_job(&JobId("j2".into()), 1_000_000);
        assert_eq!(engine.total_liability_oru(), 1_000_000);
        let round2 = engine
            .rounds()
            .into_iter()
            .find(|r| r.round_seq == seq2)
            .unwrap();
        assert!(round2.lines.is_empty());
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 250_000);
        assert_eq!(engine.balance("bob").unwrap().oru_pending, 750_000);

        // A fresh share opens a new window and is paid exactly once.
        engine.credit_share("carol", &JobId("j3".into()), "s3", 5.0);
        engine.record_block_found_for_job(&JobId("j3".into()), 400);
        assert_eq!(engine.balance("carol").unwrap().oru_pending, 400);
        assert_eq!(engine.total_liability_oru(), 1_000_400);
        assert_eq!(engine.snapshot().settled_share_count, 3);
    }

    /// C-6a: ten consecutive blocks must not inflate liabilities beyond the sum of
    /// the coinbases actually finalized.
    #[test]
    fn c6a_liabilities_do_not_diverge_over_many_blocks() {
        let engine = PayoutEngine::new(1);
        for i in 0..10u32 {
            let job = JobId(format!("j{i}"));
            engine.credit_share("alice", &job, &format!("s{i}"), 1.0);
            engine.record_block_found_for_job(&job, 100);
        }
        assert_eq!(engine.total_liability_oru(), 1_000);
        assert_eq!(engine.active_share_count(), 0);
    }

    /// H-5e regression: integer split conserves every atom, unlike the previous
    /// `f64` truncation which silently destroyed value.
    #[test]
    fn h5e_integer_split_conserves_every_atom() {
        for pot in [1i64, 7, 100, 999_999_999, 5_000_000_000] {
            let engine = PayoutEngine::new(1);
            let job = JobId("j".into());
            engine.credit_share("alice", &job, "s1", 1.0);
            engine.credit_share("bob", &job, "s2", 1.0);
            engine.credit_share("carol", &job, "s3", 1.0);
            let seq = engine.record_block_found_for_job(&job, pot);
            let round = engine
                .rounds()
                .into_iter()
                .find(|r| r.round_seq == seq)
                .unwrap();
            let paid: i64 = round.lines.iter().map(|l| l.amount_oru).sum();
            assert_eq!(paid, pot, "pot {pot} not fully distributed");
            assert_eq!(engine.total_liability_oru(), pot);
        }
    }

    /// H-5e: the remainder distribution is deterministic, not order-dependent.
    #[test]
    fn h5e_split_is_deterministic_across_credit_order() {
        let mk = |order: [(&str, f64); 3]| {
            let engine = PayoutEngine::new(1);
            let job = JobId("j".into());
            for (i, (w, d)) in order.iter().enumerate() {
                engine.credit_share(w, &job, &format!("s{i}"), *d);
            }
            engine.record_block_found_for_job(&job, 1_000_003);
            engine.rounds().remove(0).lines
        };
        let a = mk([("alice", 1.0), ("bob", 1.0), ("carol", 1.0)]);
        let b = mk([("carol", 1.0), ("alice", 1.0), ("bob", 1.0)]);
        assert_eq!(a, b);
        assert_eq!(a.iter().map(|l| l.amount_oru).sum::<i64>(), 1_000_003);
    }

    /// H-5e: fractional difficulties are quantised without producing zero weight.
    #[test]
    fn h5e_fractional_difficulty_keeps_positive_weight() {
        assert_eq!(difficulty_to_weight(1.0), DIFFICULTY_SCALE);
        assert_eq!(difficulty_to_weight(0.5), DIFFICULTY_SCALE / 2);
        assert_eq!(difficulty_to_weight(0.0), 0);
        assert_eq!(difficulty_to_weight(-3.0), 0);
        assert_eq!(difficulty_to_weight(f64::NAN), 0);
        // Tiny but positive difficulty must not round away to a free share.
        assert_eq!(difficulty_to_weight(1e-12), 1);
    }

    /// C-6e regression: the legacy anonymous round id can never be rolled back by
    /// a reorg, so it must remain deprecated and unused by production paths.
    #[test]
    fn c6e_anonymous_round_cannot_be_reversed_by_reorg() {
        let engine = PayoutEngine::new(7);
        let real_job = JobId("real-job".into());
        engine.credit_share("alice", &real_job, "s1", 1.0);
        #[allow(deprecated)]
        engine.record_block_found(100);
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 100);

        // Reorg of the real job id finds nothing to reverse: the round was booked
        // under "anon-7". This is exactly the divergence C-6e describes.
        let report = engine.reverse_finalized_for_jobs(&[real_job.clone()]);
        assert_eq!(report.rounds_reversed, 0);
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 100);

        // Bound to the real job id, rollback works.
        let engine2 = PayoutEngine::new(7);
        engine2.credit_share("alice", &real_job, "s1", 1.0);
        engine2.record_block_found_for_job(&real_job, 100);
        let report2 = engine2.reverse_finalized_for_jobs(&[real_job]);
        assert_eq!(report2.rounds_reversed, 1);
        assert_eq!(engine2.balance("alice").unwrap().oru_pending, 0);
    }

    #[test]
    fn reorg01_reverses_finalized_without_duplicate_or_loss() {
        let engine = PayoutEngine::new(42);
        let job_keep = JobId("keep".into());
        let job_drop = JobId("drop".into());

        // Round keep: alice-only window
        engine.credit_share("alice", &job_keep, "s1", 10.0);
        engine.record_block_found_for_job(&job_keep, 100);
        // Soft-orphan keep shares so they leave the active PPLNS window (round stays FINALIZED)
        engine.orphan_round_for_jobs(&[job_keep.clone()]);

        // Round drop: bob-only window
        engine.credit_share("bob", &job_drop, "s2", 10.0);
        engine.record_block_found_for_job(&job_drop, 100);

        let before = engine.ledger_state_hash();
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 100);
        assert_eq!(engine.balance("bob").unwrap().oru_pending, 100);

        let report = engine.reverse_finalized_for_jobs(&[job_drop.clone()]);
        assert_eq!(report.rounds_reversed, 1);
        assert_eq!(engine.balance("bob").unwrap().oru_pending, 0);
        // alice keep round untouched
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 100);
        let rounds = engine.rounds();
        assert!(rounds
            .iter()
            .any(|r| r.job_id == job_drop && r.status == PayoutRoundStatus::Reversed));
        assert!(rounds
            .iter()
            .any(|r| r.job_id == job_keep && r.status == PayoutRoundStatus::Finalized));

        // Idempotent second reverse: no further clawback / no duplicate reverse
        let report2 = engine.reverse_finalized_for_jobs(&[job_drop]);
        assert_eq!(report2.rounds_reversed, 0);
        assert_eq!(engine.balance("bob").unwrap().oru_pending, 0);
        assert_eq!(engine.balance("alice").unwrap().oru_pending, 100);
        assert_ne!(before, engine.ledger_state_hash());
    }

    #[test]
    fn ledger_hash_deterministic() {
        let a = PayoutEngine::new(1);
        let b = PayoutEngine::new(1);
        for e in [&a, &b] {
            e.credit_share("w", &JobId("j".into()), "s", 1.0);
            e.record_block_found_for_job(&JobId("j".into()), 50);
        }
        assert_eq!(a.ledger_state_hash(), b.ledger_state_hash());
    }
}
