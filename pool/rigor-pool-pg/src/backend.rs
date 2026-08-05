use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::error::PgError;
use crate::metrics::{MetricsSnapshot, PersistenceMetrics};
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

/// PostgreSQL backend handle; `None` means use in-memory fallback in each crate adapter.
pub struct PoolBackend {
    #[cfg(feature = "postgres")]
    pool: Option<PgPool>,
    metrics: PersistenceMetrics,
}

impl PoolBackend {
    /// Open from `DATABASE_URL` when set; returns backend with `is_postgres() == false` otherwise.
    pub fn open() -> PgResult<Self> {
        #[cfg(feature = "postgres")]
        {
            if let Ok(url) = std::env::var("DATABASE_URL") {
                if !url.is_empty() {
                    let pool = PgPool::connect_and_migrate(&url)?;
                    log::info!("rigor-pool-pg: using PostgreSQL backend");
                    return Ok(Self {
                        pool: Some(pool),
                        metrics: PersistenceMetrics::new(),
                    });
                }
            }
        }
        log::info!("rigor-pool-pg: in-memory fallback (DATABASE_URL unset)");
        Ok(Self {
            #[cfg(feature = "postgres")]
            pool: None,
            metrics: PersistenceMetrics::new(),
        })
    }

    pub fn memory() -> Self {
        Self {
            #[cfg(feature = "postgres")]
            pool: None,
            metrics: PersistenceMetrics::new(),
        }
    }

    #[cfg(feature = "postgres")]
    pub fn postgres_url(url: &str) -> PgResult<Self> {
        let pool = PgPool::connect_and_migrate(url)?;
        Ok(Self {
            pool: Some(pool),
            metrics: PersistenceMetrics::new(),
        })
    }

    pub fn is_postgres(&self) -> bool {
        #[cfg(feature = "postgres")]
        {
            self.pool.is_some()
        }
        #[cfg(not(feature = "postgres"))]
        {
            false
        }
    }

    #[cfg(feature = "postgres")]
    pub fn pool(&self) -> PgResult<&PgPool> {
        self.pool
            .as_ref()
            .ok_or_else(|| PgError::NotAvailable("PostgreSQL backend not configured".into()))
    }

    #[cfg(feature = "postgres")]
    pub fn pool_cloned(&self) -> PgResult<PgPool> {
        Ok(self.pool()?.clone())
    }

    pub fn metrics_snapshot(&self) -> MetricsSnapshot {
        self.metrics.snapshot()
    }

    pub fn record_memory_fallback(&self) {
        self.metrics.record_memory_fallback();
    }
}
