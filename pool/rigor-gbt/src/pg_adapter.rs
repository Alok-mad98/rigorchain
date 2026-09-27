//! PostgreSQL adapter for GBT template archive and longpollid audit.

use crate::cache::TemplateCache;
use crate::error::{GbtError, GbtResult};
use crate::record::TemplateRecord;
use crate::types::TipHash;

pub struct GbtPgAdapter {
    backend: rigor_pool_pg::PoolBackend,
}

impl GbtPgAdapter {
    pub fn open() -> GbtResult<Self> {
        let backend = rigor_pool_pg::PoolBackend::open()
            .map_err(|e| GbtError::Cache(e.to_string()))?;
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

    pub fn archive_template(
        &self,
        record: &TemplateRecord,
        raw_json: &str,
        payload_hash: &str,
    ) -> GbtResult<()> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(|e| GbtError::Cache(e.to_string()))?;
            let gbt_store = rigor_pool_pg::PgGbtStore::new(pool);
            let gbt_json: serde_json::Value = serde_json::from_str(raw_json)?;
            gbt_store
                .archive_template(
                    record.record_id,
                    &record.tip_hash.0,
                    &record.gbt.longpollid,
                    &gbt_json,
                    payload_hash,
                )
                .map_err(|e| GbtError::Cache(e.to_string()))?;
        }
        let _ = (record, raw_json, payload_hash);
        Ok(())
    }

    pub fn restore_last_longpollid(&self, tip: &TipHash) -> GbtResult<Option<String>> {
        #[cfg(feature = "postgres")]
        if self.backend.is_postgres() {
            let pool = self
                .backend
                .pool_cloned()
                .map_err(|e| GbtError::Cache(e.to_string()))?;
            let gbt_store = rigor_pool_pg::PgGbtStore::new(pool);
            return gbt_store
                .get_last_longpollid(&tip.0)
                .map_err(|e| GbtError::Cache(e.to_string()));
        }
        let _ = tip;
        Ok(None)
    }

    pub fn rehydrate_cache_tip(&self, _cache: &TemplateCache, _tip: TipHash) {}
}
