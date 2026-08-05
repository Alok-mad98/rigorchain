#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

#[cfg(feature = "postgres")]
pub struct PgPayoutStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgPayoutStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn open_round(&self, candidate_block_id: i64, coinbase_oru_atoms: i64) -> PgResult<i64> {
        let mut client = self.pool.get()?;
        let row = client.query_one(
            "INSERT INTO payout_rounds (candidate_block_id, coinbase_oru_atoms, status) \
             VALUES ($1, $2, 'OPEN') ON CONFLICT (candidate_block_id) DO NOTHING \
             RETURNING id",
            &[&candidate_block_id, &coinbase_oru_atoms],
        )?;
        self.pool.metrics.record_pg_insert();
        Ok(row.get(0))
    }

    pub fn insert_line(&self, round_id: i64, worker_id: i64, oru_atoms: i64) -> PgResult<()> {
        let mut client = self.pool.get()?;
        client.execute(
            "INSERT INTO payout_lines (payout_round_id, worker_id, oru_atoms) \
             VALUES ($1, $2, $3) ON CONFLICT (payout_round_id, worker_id) DO UPDATE \
             SET oru_atoms = EXCLUDED.oru_atoms",
            &[&round_id, &worker_id, &oru_atoms],
        )?;
        Ok(())
    }

    pub fn orphan_open_rounds(&self) -> PgResult<u64> {
        let mut client = self.pool.get()?;
        let n = client.execute(
            "UPDATE payout_rounds SET status = 'ORPHANED' WHERE status = 'OPEN'",
            &[],
        )?;
        self.pool.metrics.record_pg_reorg();
        Ok(n)
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgPayoutStore;
