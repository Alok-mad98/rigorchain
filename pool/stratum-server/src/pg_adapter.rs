//! PostgreSQL adapter for shares, PPLNS, workers, and audit (TASK-MINING-017).

use crate::audit::{AuditEvent, AuditLog};
use crate::error::{StratumError, StratumResult};
use crate::payout::PayoutEngine;

pub struct StratumPgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl StratumPgAdapter {
    pub fn open() -> StratumResult<Self> {
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

    pub fn ensure_worker(&self, miner: &str, worker: &str) -> StratumResult<i64> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let worker_store = rigor_pool_pg::PgWorkerStore::new(pool);
            return worker_store.ensure_worker(miner, worker).map_err(pg_err);
        }
        let _ = (miner, worker);
        Ok(0)
    }

    pub fn persist_audit(&self, event: &AuditEvent) -> StratumResult<()> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let audit_store = rigor_pool_pg::PgAuditStore::new(pool);
            audit_store
                .record(
                    &event.category,
                    &event.message,
                    serde_json::json!({ "timestamp": event.timestamp }),
                    None,
                    None,
                    None,
                )
                .map_err(pg_err)?;
        }
        let _ = event;
        Ok(())
    }

    pub fn flush_audit_log(&self, log: &AuditLog) -> StratumResult<usize> {
        let events = log.history();
        for e in &events {
            self.persist_audit(e)?;
        }
        Ok(events.len())
    }

    pub fn open_payout_round(
        &self,
        _payout: &PayoutEngine,
        candidate_block_id: i64,
        coinbase_oru_atoms: i64,
    ) -> StratumResult<i64> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let payout_store = rigor_pool_pg::PgPayoutStore::new(pool);
            return payout_store
                .open_round(candidate_block_id, coinbase_oru_atoms)
                .map_err(pg_err);
        }
        let _ = (_payout, candidate_block_id, coinbase_oru_atoms);
        Ok(0)
    }

    pub fn metrics(&self) -> rigor_pool_pg::MetricsSnapshot {
        self.backend.metrics_snapshot()
    }
}

fn pg_err(e: rigor_pool_pg::PgError) -> StratumError {
    StratumError::Internal(e.to_string())
}
