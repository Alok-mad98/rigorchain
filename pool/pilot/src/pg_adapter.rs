//! PostgreSQL adapter for pilot orchestration status snapshots (TASK-MINING-017).

use crate::{PilotError, PilotResult, PilotStatusReport};

pub struct PilotPgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl PilotPgAdapter {
    pub fn open() -> PilotResult<Self> {
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

    pub fn persist_status(&self, report: &PilotStatusReport) -> PilotResult<()> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(pg_err)?;
            let mut client = pool.get().map_err(pg_err)?;
            let json = serde_json::to_value(report).map_err(|e| PilotError::Config(e.to_string()))?;
            client
                .execute(
                    "INSERT INTO pilot_status_snapshots (snapshot_jsonb) VALUES ($1)",
                    &[&json],
                )
                .map_err(pg_err)?;
            return Ok(());
        }
        let _ = report;
        Ok(())
    }

    pub fn run_recovery(&mut self, chain_tip: &str) -> PilotResult<rigor_pool_pg::RecoveryReport> {
        rigor_pool_pg::RecoveryEngine::check_tip_and_recover(&mut self.backend, chain_tip)
            .map_err(pg_err)
    }

    pub fn metrics(&self) -> rigor_pool_pg::MetricsSnapshot {
        self.backend.metrics_snapshot()
    }
}

fn pg_err(e: rigor_pool_pg::PgError) -> PilotError {
    PilotError::Config(e.to_string())
}
