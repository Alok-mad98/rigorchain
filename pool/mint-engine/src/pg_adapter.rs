//! PostgreSQL adapter for mint_jobs and treasury reservations (TASK-MINING-017).

use crate::error::{MintError, MintResult};
use crate::state::MintStateStore;
use crate::types::{BodyMintArtifact, MintJobRef};

pub struct MintPgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl MintPgAdapter {
    pub fn open() -> MintResult<Self> {
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

    #[cfg_attr(not(feature = "postgres"), allow(unused_variables))]
    pub fn register_built_atomic(
        &self,
        memory: &MintStateStore,
        mint_job: MintJobRef,
        artifact: BodyMintArtifact,
        utxo: rigor_pool_pg::TreasuryReserveInput,
        now: u64,
    ) -> MintResult<i64> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let mint_store = rigor_pool_pg::PgMintStore::new(pool);
            let pg_id = mint_store
                .reserve_mint_with_utxo(
                    &mint_job.job_id,
                    None,
                    &artifact.body_mint_txid,
                    artifact.raw_tx_hex.as_bytes(),
                    &artifact.sigma_digest_hex,
                    &utxo,
                )
                .map_err(pg_err)?;
            memory.register_built(mint_job, artifact, now)?;
            return Ok(pg_id);
        }
        memory.register_built(mint_job, artifact, now)?;
        Ok(0)
    }

    pub fn rehydrate_inflight(&self, _memory: &MintStateStore) -> MintResult<Vec<String>> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self.backend.pool_cloned().map_err(pg_err)?;
            let mint_store = rigor_pool_pg::PgMintStore::new(pool);
            return mint_store.list_inflight_job_ids().map_err(pg_err);
        }
        Ok(Vec::new())
    }
}

fn pg_err(e: rigor_pool_pg::PgError) -> MintError {
    MintError::State(e.to_string())
}
