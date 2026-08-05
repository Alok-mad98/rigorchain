#[cfg(feature = "postgres")]
use serde_json::Value;

#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

/// Persists immutable job snapshot JSONB rows.
#[cfg(feature = "postgres")]
pub struct PgJobSnapshotStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgJobSnapshotStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn insert_snapshot(
        &self,
        job_id: &str,
        record_id: uuid::Uuid,
        prev_block_hash: &str,
        tip_hash: &str,
        height: u64,
        convergence_root: &str,
        mint_required: bool,
        body_mint_reserved: bool,
        status: &str,
        snapshot: &Value,
    ) -> PgResult<i64> {
        let mut client = self.pool.get()?;
        let row = client.query_one(
            "INSERT INTO candidate_blocks \
             (job_id, record_id, prev_block_hash, tip_hash, height, convergence_root, \
              mint_required, body_mint_reserved, status, snapshot_jsonb) \
             VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) \
             ON CONFLICT (job_id) DO UPDATE SET snapshot_jsonb = EXCLUDED.snapshot_jsonb \
             RETURNING id",
            &[
                &job_id,
                &record_id,
                &prev_block_hash,
                &tip_hash,
                &(height as i32),
                &convergence_root,
                &mint_required,
                &body_mint_reserved,
                &status,
                &snapshot,
            ],
        )?;
        self.pool.metrics.record_pg_insert();
        log::info!("pg: insert job snapshot job_id={job_id}");
        Ok(row.get(0))
    }

    pub fn get_snapshot_json(&self, job_id: &str) -> PgResult<Option<Value>> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT snapshot_jsonb FROM candidate_blocks WHERE job_id = $1",
            &[&job_id],
        )?;
        Ok(rows.first().map(|r| r.get(0)))
    }

    pub fn next_job_serial(&self) -> PgResult<u64> {
        let mut client = self.pool.get()?;
        let row = client.query_one(
            "UPDATE pool_sequence SET next_value = next_value + 1 WHERE name = 'job_serial' \
             RETURNING next_value - 1",
            &[],
        )?;
        Ok(row.get::<_, i64>(0) as u64)
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgJobSnapshotStore;
