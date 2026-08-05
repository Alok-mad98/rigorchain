use crate::backend::PoolBackend;
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;
#[cfg(feature = "postgres")]
use crate::stores::{mint::PgMintStore, payout::PgPayoutStore, share::PgShareStore};

/// Startup recovery per POSTGRESQL_PERSISTENCE_ARCHITECTURE.md §6.
pub struct RecoveryEngine;

impl RecoveryEngine {
    pub fn check_tip_and_recover(
        backend: &mut PoolBackend,
        chain_tip: &str,
    ) -> PgResult<RecoveryReport> {
        if !backend.is_postgres() {
            backend.record_memory_fallback();
            let _ = chain_tip;
            return Ok(RecoveryReport::default());
        }
        #[cfg(feature = "postgres")]
        {
            Self::pg_recover(backend, chain_tip)
        }
        #[cfg(not(feature = "postgres"))]
        {
            let _ = chain_tip;
            Ok(RecoveryReport::default())
        }
    }

    #[cfg(feature = "postgres")]
    fn pg_recover(backend: &PoolBackend, chain_tip: &str) -> PgResult<RecoveryReport> {
        let pool = backend.pool_cloned()?;
        let mut client = pool.get()?;
        let cursor_rows = client.query(
            "SELECT current_tip FROM pool_chain_cursor WHERE id = 1",
            &[],
        )?;
        let stored_tip: String = cursor_rows
            .first()
            .map(|r| r.get(0))
            .unwrap_or_default();

        let mut report = RecoveryReport::default();
        if !stored_tip.is_empty() && !stored_tip.eq_ignore_ascii_case(chain_tip) {
            report.tip_mismatch = true;
            let _reorg_id = Self::insert_reorg_event(&mut client, &stored_tip, chain_tip)?;
            Self::run_reorg_handler(&pool, &stored_tip)?;
            report.reorg_applied = true;
        }

        client.execute(
            "UPDATE pool_chain_cursor SET current_tip = $1, last_gbt_at = now() WHERE id = 1",
            &[&chain_tip],
        )?;

        let active_rows = client.query(
            "SELECT COUNT(*) FROM candidate_blocks WHERE status = 'ACTIVE'",
            &[],
        )?;
        report.active_candidates = active_rows[0].get::<_, i64>(0) as usize;

        let mint_store = PgMintStore::new(pool.clone());
        report.inflight_mints = mint_store.list_inflight_job_ids()?.len();

        let share_store = PgShareStore::new(pool.clone());
        report.replay_shares_loaded = share_store.load_replay_set(100_000)?.len();

        log::info!(
            "pg recovery: tip_mismatch={} active={} inflight_mints={} replay_shares={}",
            report.tip_mismatch,
            report.active_candidates,
            report.inflight_mints,
            report.replay_shares_loaded
        );
        Ok(report)
    }

    #[cfg(feature = "postgres")]
    fn insert_reorg_event(
        client: &mut postgres::Client,
        old_tip: &str,
        new_tip: &str,
    ) -> PgResult<i64> {
        let row = client.query_one(
            "INSERT INTO reorg_events (old_tip, new_tip) VALUES ($1, $2) \
             ON CONFLICT (old_tip, new_tip) DO UPDATE SET detected_at = now() RETURNING id",
            &[&old_tip, &new_tip],
        )?;
        Ok(row.get(0))
    }

    #[cfg(feature = "postgres")]
    fn run_reorg_handler(pool: &PgPool, old_tip: &str) -> PgResult<()> {
        let mut client = pool.get()?;
        client.execute(
            "UPDATE candidate_blocks SET status = 'ORPHANED' WHERE prev_block_hash = $1 AND status = 'ACTIVE'",
            &[&old_tip],
        )?;
        let mint = PgMintStore::new(pool.clone());
        mint.revoke_inflight_mints()?;
        let share = PgShareStore::new(pool.clone());
        let cand_rows = client.query(
            "SELECT id FROM candidate_blocks WHERE status = 'ORPHANED' AND prev_block_hash = $1",
            &[&old_tip],
        )?;
        let ids: Vec<i64> = cand_rows.iter().map(|r| r.get(0)).collect();
        share.orphan_shares_for_candidates(&ids)?;
        let payout = PgPayoutStore::new(pool.clone());
        payout.orphan_open_rounds()?;
        pool.metrics.record_pg_reorg();
        log::info!("pg: reorg handler applied for old_tip={old_tip}");
        Ok(())
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct RecoveryReport {
    pub tip_mismatch: bool,
    pub reorg_applied: bool,
    pub active_candidates: usize,
    pub inflight_mints: usize,
    pub replay_shares_loaded: usize,
}
