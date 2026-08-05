#[cfg(feature = "postgres")]
use uuid::Uuid;

#[cfg(feature = "postgres")]
use crate::error::{WinnerError, WinnerResult};
#[cfg(feature = "postgres")]
use crate::persist::traits::{
    CandidateRepository, MintAuthorizationRepository, WinnerRepository,
};
#[cfg(feature = "postgres")]
use crate::types::{
    CandidateBlockRecord, CandidateBlockStatus, DrawReplayResult, MintAuthorizationToken,
    WinnerJobRecord, WinnerJobStatus,
};
#[cfg(feature = "postgres")]
use rigor_gbt::RecordId;
#[cfg(feature = "postgres")]
use rigor_pool_pg::{MetricsSnapshot, PgPool};

#[cfg(feature = "postgres")]
fn status_to_str(s: WinnerJobStatus) -> &'static str {
    match s {
        WinnerJobStatus::Provisional => "PROVISIONAL",
        WinnerJobStatus::Validated => "VALIDATED",
        WinnerJobStatus::Authorized => "AUTHORIZED",
        WinnerJobStatus::Confirmed => "CONFIRMED",
        WinnerJobStatus::Stale => "STALE",
        WinnerJobStatus::Forbidden => "FORBIDDEN",
        WinnerJobStatus::Revoked => "REVOKED",
        WinnerJobStatus::ConfirmationPending => "CONFIRMATION_PENDING",
        WinnerJobStatus::ConfirmationDrift => "CONFIRMATION_DRIFT",
    }
}

#[cfg(feature = "postgres")]
fn str_to_status(s: &str) -> WinnerJobStatus {
    match s {
        "VALIDATED" => WinnerJobStatus::Validated,
        "AUTHORIZED" => WinnerJobStatus::Authorized,
        "CONFIRMED" => WinnerJobStatus::Confirmed,
        "STALE" => WinnerJobStatus::Stale,
        "FORBIDDEN" => WinnerJobStatus::Forbidden,
        "REVOKED" => WinnerJobStatus::Revoked,
        "CONFIRMATION_PENDING" => WinnerJobStatus::ConfirmationPending,
        "CONFIRMATION_DRIFT" => WinnerJobStatus::ConfirmationDrift,
        _ => WinnerJobStatus::Provisional,
    }
}

#[cfg(feature = "postgres")]
fn candidate_status_to_str(s: CandidateBlockStatus) -> &'static str {
    match s {
        CandidateBlockStatus::Active => "ACTIVE",
        CandidateBlockStatus::Found => "FOUND",
        CandidateBlockStatus::Orphaned => "ORPHANED",
        CandidateBlockStatus::Expired => "EXPIRED",
        CandidateBlockStatus::Submitted => "SUBMITTED",
        CandidateBlockStatus::Rejected => "REJECTED",
    }
}

#[cfg(feature = "postgres")]
fn str_to_candidate_status(s: &str) -> CandidateBlockStatus {
    match s {
        "FOUND" => CandidateBlockStatus::Found,
        "ORPHANED" => CandidateBlockStatus::Orphaned,
        "EXPIRED" => CandidateBlockStatus::Expired,
        "SUBMITTED" => CandidateBlockStatus::Submitted,
        "REJECTED" => CandidateBlockStatus::Rejected,
        _ => CandidateBlockStatus::Active,
    }
}

#[cfg(feature = "postgres")]
fn pg_err(e: rigor_pool_pg::PgError) -> WinnerError {
    WinnerError::Persist(e.to_string())
}

#[cfg(feature = "postgres")]
fn db_err(e: postgres::Error) -> WinnerError {
    WinnerError::Persist(e.to_string())
}

/// PostgreSQL winner / candidate / authorization store.
#[cfg(feature = "postgres")]
pub struct PgWinnerStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgWinnerStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn metrics_snapshot(&self) -> MetricsSnapshot {
        self.pool.metrics.snapshot()
    }

    fn row_to_winner_job(
        id: i64,
        candidate_block_id: i64,
        status: &str,
        mint_required: bool,
        eligible_count: i32,
        winner_txid: Option<String>,
        preview_audit: serde_json::Value,
        token_nonce: Option<Uuid>,
    ) -> WinnerResult<WinnerJobRecord> {
        let preview: DrawReplayResult = serde_json::from_value(preview_audit)
            .map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(WinnerJobRecord {
            id,
            candidate_block_id,
            status: str_to_status(status),
            mint_required,
            eligible_count: eligible_count as u32,
            convergence_root_display: preview.convergence_root_display.clone(),
            winner_txid_display: winner_txid,
            preview_audit: preview,
            token_nonce,
        })
    }
}

#[cfg(feature = "postgres")]
impl WinnerRepository for PgWinnerStore {
    fn insert_winner_job(&mut self, job: WinnerJobRecord) -> WinnerResult<i64> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let audit = serde_json::to_value(&job.preview_audit)
            .map_err(|e| WinnerError::Persist(e.to_string()))?;
        let txids: Vec<String> = job
            .preview_audit
            .candidates
            .iter()
            .map(|c| c.txid_display.clone())
            .collect();
        let txids_json = serde_json::to_value(&txids)
            .map_err(|e| WinnerError::Persist(e.to_string()))?;
        let row = client
            .query_one(
                "INSERT INTO winner_jobs (candidate_block_id, status, mint_required, eligible_count, \
                 winner_txid, convergence_txids, preview_audit, token_nonce) \
                 VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id",
                &[
                    &job.candidate_block_id,
                    &status_to_str(job.status),
                    &job.mint_required,
                    &(job.eligible_count as i32),
                    &job.winner_txid_display,
                    &txids_json,
                    &audit,
                    &job.token_nonce,
                ],
            )
            .map_err(db_err)?;
        self.pool.metrics.record_pg_insert();
        log::info!(
            "pg: insert winner_job candidate_block_id={} id={}",
            job.candidate_block_id,
            row.get::<_, i64>(0)
        );
        Ok(row.get(0))
    }

    fn update_status(&mut self, id: i64, status: WinnerJobStatus) -> WinnerResult<()> {
        let mut client = self.pool.get().map_err(pg_err)?;
        client
            .execute(
                "UPDATE winner_jobs SET status = $1 WHERE id = $2",
                &[&status_to_str(status), &id],
            )
            .map_err(db_err)?;
        self.pool.metrics.record_pg_update();
        Ok(())
    }

    fn get_by_candidate_block(
        &self,
        candidate_block_id: i64,
    ) -> WinnerResult<Option<WinnerJobRecord>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT id, candidate_block_id, status, mint_required, eligible_count, \
                 winner_txid, preview_audit, token_nonce FROM winner_jobs \
                 WHERE candidate_block_id = $1",
                &[&candidate_block_id],
            )
            .map_err(db_err)?;
        if rows.is_empty() {
            return Ok(None);
        }
        let r = &rows[0];
        let status: String = r.get(2);
        Self::row_to_winner_job(
            r.get(0),
            r.get(1),
            &status,
            r.get(3),
            r.get(4),
            r.get(5),
            r.get(6),
            r.get(7),
        )
        .map(Some)
    }

    fn list_by_status(&self, status: WinnerJobStatus) -> WinnerResult<Vec<WinnerJobRecord>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT id, candidate_block_id, status, mint_required, eligible_count, \
                 winner_txid, preview_audit, token_nonce FROM winner_jobs WHERE status = $1",
                &[&status_to_str(status)],
            )
            .map_err(db_err)?;
        rows.iter()
            .map(|r| {
                let status: String = r.get(2);
                Self::row_to_winner_job(
                    r.get(0),
                    r.get(1),
                    &status,
                    r.get(3),
                    r.get(4),
                    r.get(5),
                    r.get(6),
                    r.get(7),
                )
            })
            .collect()
    }
}

#[cfg(feature = "postgres")]
impl MintAuthorizationRepository for PgWinnerStore {
    fn save_token(&mut self, token: MintAuthorizationToken) -> WinnerResult<()> {
        let mut client = self.pool.get().map_err(pg_err)?;
        client
            .execute(
                "INSERT INTO mint_authorization_tokens \
                 (token_nonce, winner_job_id, candidate_block_id, draw_winner_txid, convergence_root, \
                  bind_hash, target_prev, height, winner_convergence_amount, res_payout_program, \
                  res_probability_target, authorized_at, expires_at, replay_cert_tag, status, consumed) \
                 VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16) \
                 ON CONFLICT (token_nonce) DO UPDATE SET status = EXCLUDED.status",
                &[
                    &token.token_nonce,
                    &token.winner_job_id,
                    &token.candidate_block_id,
                    &token.draw_winner_txid_display,
                    &token.convergence_root_display,
                    &token.bind_hash_display,
                    &(token.target_prev as i32),
                    &(token.height as i32),
                    &(token.winner_convergence_amount as i64),
                    &hex::encode(token.res_payout_program),
                    &hex::encode(token.res_probability_target),
                    &(token.authorized_at as i64),
                    &(token.expires_at as i64),
                    &token.replay_cert_tag,
                    &status_to_str(token.status),
                    &false,
                ],
            )
            .map_err(db_err)?;
        self.pool.metrics.record_pg_insert();
        Ok(())
    }

    fn get_token(&self, nonce: Uuid) -> WinnerResult<Option<MintAuthorizationToken>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT token_nonce, winner_job_id, candidate_block_id, draw_winner_txid, \
                 convergence_root, bind_hash, target_prev, height, winner_convergence_amount, \
                 res_payout_program, res_probability_target, authorized_at, expires_at, \
                 replay_cert_tag, status \
                 FROM mint_authorization_tokens WHERE token_nonce = $1",
                &[&nonce],
            )
            .map_err(db_err)?;
        if rows.is_empty() {
            return Ok(None);
        }
        Ok(Some(row_to_token(&rows[0])?))
    }

    fn revoke_by_candidate_block(&mut self, candidate_block_id: i64) -> WinnerResult<usize> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let n = client
            .execute(
                "UPDATE mint_authorization_tokens SET status = 'REVOKED' \
                 WHERE candidate_block_id = $1 AND status = 'AUTHORIZED'",
                &[&candidate_block_id],
            )
            .map_err(db_err)?;
        Ok(n as usize)
    }

    fn is_nonce_consumed(&self, nonce: Uuid) -> bool {
        self.pool
            .get()
            .ok()
            .and_then(|mut c| {
                c.query(
                    "SELECT consumed FROM mint_authorization_tokens WHERE token_nonce = $1",
                    &[&nonce],
                )
                .ok()
            })
            .and_then(|rows| rows.first().map(|r| r.get::<_, bool>(0)))
            .unwrap_or(false)
    }

    fn mark_nonce_consumed(&mut self, nonce: Uuid) -> WinnerResult<()> {
        let mut client = self.pool.get().map_err(pg_err)?;
        client
            .execute(
                "UPDATE mint_authorization_tokens SET consumed = true WHERE token_nonce = $1",
                &[&nonce],
            )
            .map_err(db_err)?;
        Ok(())
    }

    fn get_token_by_candidate_block(
        &self,
        candidate_block_id: i64,
    ) -> WinnerResult<Option<MintAuthorizationToken>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT token_nonce, winner_job_id, candidate_block_id, draw_winner_txid, \
                 convergence_root, bind_hash, target_prev, height, winner_convergence_amount, \
                 res_payout_program, res_probability_target, authorized_at, expires_at, \
                 replay_cert_tag, status \
                 FROM mint_authorization_tokens WHERE candidate_block_id = $1 \
                 ORDER BY authorized_at DESC LIMIT 1",
                &[&candidate_block_id],
            )
            .map_err(db_err)?;
        if rows.is_empty() {
            return Ok(None);
        }
        Ok(Some(row_to_token(&rows[0])?))
    }
}

#[cfg(feature = "postgres")]
fn row_to_token(r: &postgres::Row) -> WinnerResult<MintAuthorizationToken> {
    let res_hex: String = r.get(10);
    let mut res_target = [0u8; 32];
    if let Ok(bytes) = hex::decode(res_hex.trim()) {
        let len = bytes.len().min(32);
        res_target[..len].copy_from_slice(&bytes[..len]);
    }
    let draw_hex: String = r.get(3);
    let cr_hex: String = r.get(4);
    let bind_hex: String = r.get(5);
    Ok(MintAuthorizationToken {
        winner_job_id: r.get(1),
        candidate_block_id: r.get(2),
        draw_winner_txid: parse_hex32(&draw_hex),
        draw_winner_txid_display: draw_hex,
        convergence_root: parse_hex32(&cr_hex),
        convergence_root_display: cr_hex,
        bind_hash: parse_hex32(&bind_hex),
        bind_hash_display: bind_hex,
        target_prev: r.get::<_, i32>(6) as u32,
        height: r.get::<_, i32>(7) as u32,
        winner_convergence_amount: r.get::<_, i64>(8) as u64,
        res_payout_program: parse_hex32(&r.get::<_, String>(9)),
        res_probability_target: res_target,
        authorized_at: r.get::<_, i64>(11) as u64,
        expires_at: r.get::<_, i64>(12) as u64,
        token_nonce: r.get(0),
        replay_cert_tag: r.get(13),
        status: str_to_status(&r.get::<_, String>(14)),
    })
}

#[cfg(feature = "postgres")]
fn parse_hex32(hex_str: &str) -> [u8; 32] {
    let mut out = [0u8; 32];
    if let Ok(bytes) = hex::decode(hex_str.trim()) {
        let len = bytes.len().min(32);
        out[..len].copy_from_slice(&bytes[..len]);
    }
    out
}

#[cfg(feature = "postgres")]
impl CandidateRepository for PgWinnerStore {
    fn insert_candidate(&mut self, block: CandidateBlockRecord) -> WinnerResult<i64> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let row = client
            .query_one(
                "INSERT INTO candidate_blocks \
                 (job_id, record_id, prev_block_hash, tip_hash, height, convergence_root, \
                  mint_required, body_mint_reserved, status) \
                 VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9) RETURNING id",
                &[
                    &block.job_id,
                    &block.record_id.0,
                    &block.prev_block_hash,
                    &block.tip_hash,
                    &(block.height as i32),
                    &block.convergence_root_display,
                    &block.mint_required,
                    &block.body_mint_reserved,
                    &candidate_status_to_str(block.status),
                ],
            )
            .map_err(db_err)?;
        self.pool.metrics.record_pg_insert();
        Ok(row.get(0))
    }

    fn update_status(&mut self, id: i64, status: CandidateBlockStatus) -> WinnerResult<()> {
        let mut client = self.pool.get().map_err(pg_err)?;
        client
            .execute(
                "UPDATE candidate_blocks SET status = $1 WHERE id = $2",
                &[&candidate_status_to_str(status), &id],
            )
            .map_err(db_err)?;
        Ok(())
    }

    fn get(&self, id: i64) -> WinnerResult<Option<CandidateBlockRecord>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT id, job_id, record_id, prev_block_hash, tip_hash, height, \
                 convergence_root, mint_required, body_mint_reserved, status \
                 FROM candidate_blocks WHERE id = $1",
                &[&id],
            )
            .map_err(db_err)?;
        if rows.is_empty() {
            return Ok(None);
        }
        Ok(Some(row_to_candidate(&rows[0])))
    }

    fn list_active(&self) -> WinnerResult<Vec<CandidateBlockRecord>> {
        let mut client = self.pool.get().map_err(pg_err)?;
        let rows = client
            .query(
                "SELECT id, job_id, record_id, prev_block_hash, tip_hash, height, \
                 convergence_root, mint_required, body_mint_reserved, status \
                 FROM candidate_blocks WHERE status = 'ACTIVE'",
                &[],
            )
            .map_err(db_err)?;
        Ok(rows.iter().map(row_to_candidate).collect())
    }
}

#[cfg(feature = "postgres")]
fn row_to_candidate(r: &postgres::Row) -> CandidateBlockRecord {
    CandidateBlockRecord {
        id: r.get(0),
        job_id: r.get(1),
        record_id: RecordId(r.get(2)),
        prev_block_hash: r.get(3),
        tip_hash: r.get(4),
        height: r.get::<_, i32>(5) as u64,
        convergence_root_display: r.get(6),
        mint_required: r.get(7),
        body_mint_reserved: r.get(8),
        status: str_to_candidate_status(&r.get::<_, String>(9)),
    }
}
