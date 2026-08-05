#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

#[cfg(feature = "postgres")]
pub struct PgWorkerStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgWorkerStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn ensure_worker(&self, miner_username: &str, worker_name: &str) -> PgResult<i64> {
        let mut client = self.pool.get()?;
        let miner_row = client.query_one(
            "INSERT INTO miners (username) VALUES ($1) ON CONFLICT (username) DO UPDATE \
             SET username = EXCLUDED.username RETURNING id",
            &[&miner_username],
        )?;
        let miner_id: i64 = miner_row.get(0);
        let worker_row = client.query_one(
            "INSERT INTO workers (miner_id, worker_name) VALUES ($1, $2) \
             ON CONFLICT (miner_id, worker_name) DO UPDATE SET worker_name = EXCLUDED.worker_name \
             RETURNING id",
            &[&miner_id, &worker_name],
        )?;
        Ok(worker_row.get(0))
    }

    pub fn update_difficulty(&self, worker_id: i64, difficulty: f64) -> PgResult<()> {
        let mut client = self.pool.get()?;
        client.execute(
            "UPDATE workers SET current_difficulty = $1, last_share_at = now() WHERE id = $2",
            &[&difficulty, &worker_id],
        )?;
        Ok(())
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgWorkerStore;
