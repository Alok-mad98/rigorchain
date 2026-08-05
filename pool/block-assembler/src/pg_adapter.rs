//! PostgreSQL adapter for `PublishedJobSnapshot` persistence (TASK-MINING-017).

use crate::cache::JobCache;
use crate::error::{AssemblerError, AssemblerResult};
use crate::types::{JobId, PublishedJobSnapshot};

pub struct AssemblerPgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl AssemblerPgAdapter {
    pub fn open() -> AssemblerResult<Self> {
        let backend = rigor_pool_pg::PoolBackend::open()
            .map_err(|e| AssemblerError::Publish(e.to_string()))?;
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

    pub fn allocate_job_serial(&self, cache: &JobCache) -> AssemblerResult<(JobId, u64)> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            let snap_store = rigor_pool_pg::PgJobSnapshotStore::new(pool);
            let serial = snap_store
                .next_job_serial()
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            return Ok((JobId(format!("{serial:016x}")), serial));
        }
        let (job_id, cand_id) = cache.allocate_ids()?;
        Ok((job_id, cand_id.0))
    }

    pub fn persist_snapshot(&self, snapshot: &PublishedJobSnapshot) -> AssemblerResult<i64> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            let snap_store = rigor_pool_pg::PgJobSnapshotStore::new(pool);
            let json = serde_json::to_value(snapshot)
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            return snap_store
                .insert_snapshot(
                    &snapshot.job_id.0,
                    snapshot.record_id,
                    &snapshot.previousblockhash,
                    &snapshot.tip_hash.0,
                    snapshot.height,
                    &snapshot.coinbase.convergence_root_hex,
                    snapshot.body_mint.mint_required,
                    snapshot.body_mint.reserved,
                    &format!("{:?}", snapshot.state).to_uppercase(),
                    &json,
                )
                .map_err(|e| AssemblerError::Publish(e.to_string()));
        }
        Ok(snapshot.candidate_block_id.0 as i64)
    }

    pub fn load_snapshot(&self, job_id: &JobId) -> AssemblerResult<Option<PublishedJobSnapshot>> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            let snap_store = rigor_pool_pg::PgJobSnapshotStore::new(pool);
            let val = snap_store
                .get_snapshot_json(&job_id.0)
                .map_err(|e| AssemblerError::Publish(e.to_string()))?;
            return val
                .map(|v| serde_json::from_value(v))
                .transpose()
                .map_err(|e| AssemblerError::Publish(e.to_string()));
        }
        let _ = job_id;
        Ok(None)
    }
}
