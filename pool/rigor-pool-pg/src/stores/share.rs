#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

/// Share replay set and share persistence (TASK-MINING-017 §2.3).
#[cfg(feature = "postgres")]
pub struct PgShareStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgShareStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn insert_share(
        &self,
        share_id: &str,
        worker_id: i64,
        candidate_block_id: i64,
        job_id: &str,
        share_diff: f64,
        is_block: bool,
        pow_hash: &str,
    ) -> PgResult<()> {
        let mut client = self.pool.get()?;
        client.execute(
            "INSERT INTO shares (share_id, worker_id, candidate_block_id, job_id, share_diff, \
             is_block, pow_hash) VALUES ($1,$2,$3,$4,$5,$6,$7)",
            &[
                &share_id,
                &worker_id,
                &candidate_block_id,
                &job_id,
                &share_diff,
                &is_block,
                &pow_hash,
            ],
        )?;
        self.pool.metrics.record_pg_insert();
        log::info!("pg: insert share share_id={}", share_id);
        Ok(())
    }

    pub fn is_share_seen(&self, share_id: &str) -> PgResult<bool> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT 1 FROM shares WHERE share_id = $1 LIMIT 1",
            &[&share_id],
        )?;
        Ok(!rows.is_empty())
    }

    pub fn load_replay_set(&self, limit: i64) -> PgResult<Vec<String>> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT share_id FROM shares ORDER BY created_at DESC LIMIT $1",
            &[&limit],
        )?;
        Ok(rows.iter().map(|r| r.get::<_, String>(0)).collect())
    }

    pub fn orphan_shares_for_candidates(&self, candidate_ids: &[i64]) -> PgResult<u64> {
        if candidate_ids.is_empty() {
            return Ok(0);
        }
        let mut client = self.pool.get()?;
        let n = client.execute(
            "UPDATE shares SET orphaned = true WHERE candidate_block_id = ANY($1) AND orphaned = false",
            &[&candidate_ids],
        )?;
        self.pool.metrics.record_pg_update();
        Ok(n)
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgShareStore;
