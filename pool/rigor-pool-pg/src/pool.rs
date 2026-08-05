#[cfg(feature = "postgres")]
use std::sync::Arc;

use crate::error::{PgError, PgResult};
#[cfg(feature = "postgres")]
use crate::metrics::PersistenceMetrics;

#[cfg(feature = "postgres")]
type PgConnManager = r2d2_postgres::PostgresConnectionManager<postgres::NoTls>;

/// Shared PostgreSQL connection pool (cheaply cloneable via `Arc`).
#[cfg(feature = "postgres")]
#[derive(Clone)]
pub struct PgPool {
    inner: Arc<r2d2::Pool<PgConnManager>>,
    pub metrics: Arc<PersistenceMetrics>,
}

#[cfg(feature = "postgres")]
impl PgPool {
    pub fn connect(database_url: &str) -> PgResult<Self> {
        let config: postgres::Config = database_url
            .parse::<postgres::Config>()
            .map_err(|e| PgError::Config(e.to_string()))?;
        let manager = PgConnManager::new(config, postgres::NoTls);
        let inner = r2d2::Pool::builder()
            .max_size(8)
            .build(manager)
            .map_err(PgError::from)?;
        Ok(Self {
            inner: Arc::new(inner),
            metrics: Arc::new(PersistenceMetrics::new()),
        })
    }

    pub fn from_env() -> PgResult<Self> {
        let url = std::env::var("DATABASE_URL")
            .map_err(|_| PgError::Config("DATABASE_URL not set".into()))?;
        Self::connect(&url)
    }

    pub fn get(&self) -> PgResult<r2d2::PooledConnection<PgConnManager>> {
        self.inner.get().map_err(PgError::from)
    }

    pub fn migrate(&self) -> PgResult<()> {
        let sql = include_str!("../migrations/001_schema.sql");
        let mut client = self.get()?;
        client.batch_execute(sql)?;
        log::info!("rigor-pool-pg: schema migration applied");
        Ok(())
    }

    /// Test helper: connect and migrate in one step.
    pub fn connect_and_migrate(database_url: &str) -> PgResult<Self> {
        let pool = Self::connect(database_url)?;
        pool.migrate()?;
        Ok(pool)
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgPool;

#[cfg(not(feature = "postgres"))]
impl PgPool {
    pub fn from_env() -> PgResult<Self> {
        Err(PgError::NotAvailable(
            "postgres feature disabled; rebuild with --features postgres".into(),
        ))
    }
}
