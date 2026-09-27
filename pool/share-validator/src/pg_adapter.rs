//! PostgreSQL adapter for share replay set .

use crate::error::{ValidatorError, ValidatorResult};
use crate::replay::ReplayGuard;

pub struct SharePgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl SharePgAdapter {
    pub fn open() -> ValidatorResult<Self> {
        let backend = rigor_pool_pg::PoolBackend::open().map_err(pg_err)?;
        Ok(Self { backend })
    }

    pub fn memory() -> Self {
        Self {
            backend: rigor_pool_pg::PoolBackend::memory(),
        }
    }

    pub fn is_postgres(&self) -> bool {
        self.backend.is_postgres()
    }

    pub fn check_share(&self, guard: &ReplayGuard, share_id: &str) -> ValidatorResult<bool> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let share_store = rigor_pool_pg::PgShareStore::new(pool);
            if share_store.is_share_seen(share_id).map_err(pg_err)? {
                return Ok(true);
            }
        }
        Ok(guard.is_share_seen(share_id))
    }

    pub fn rehydrate_replay_guard(&self, guard: &ReplayGuard, limit: i64) -> ValidatorResult<usize> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let share_store = rigor_pool_pg::PgShareStore::new(pool);
            let ids = share_store.load_replay_set(limit).map_err(pg_err)?;
            let n = ids.len();
            guard.import_seen_shares(ids);
            return Ok(n);
        }
        let _ = (guard, limit);
        Ok(0)
    }

    pub fn persist_share(
        &self,
        share_id: &str,
        worker_id: i64,
        candidate_block_id: i64,
        job_id: &str,
        share_diff: f64,
        is_block: bool,
        pow_hash: &str,
    ) -> ValidatorResult<()> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let share_store = rigor_pool_pg::PgShareStore::new(pool);
            share_store
                .insert_share(
                    share_id,
                    worker_id,
                    candidate_block_id,
                    job_id,
                    share_diff,
                    is_block,
                    pow_hash,
                )
                .map_err(pg_err)?;
        }
        let _ = (
            share_id,
            worker_id,
            candidate_block_id,
            job_id,
            share_diff,
            is_block,
            pow_hash,
        );
        Ok(())
    }
}

fn pg_err(e: rigor_pool_pg::PgError) -> ValidatorError {
    ValidatorError::Internal(e.to_string())
}
