#[cfg(feature = "postgres")]
use serde_json::Value;

#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

#[cfg(feature = "postgres")]
pub struct PgAuditStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgAuditStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn record(
        &self,
        category: &str,
        message: &str,
        detail: Value,
        candidate_block_id: Option<i64>,
        mint_job_id: Option<i64>,
        reorg_event_id: Option<i64>,
    ) -> PgResult<()> {
        let mut client = self.pool.get()?;
        client.execute(
            "INSERT INTO audit_events (category, message, detail, candidate_block_id, mint_job_id, reorg_event_id) \
             VALUES ($1, $2, $3, $4, $5, $6)",
            &[
                &category,
                &message,
                &detail,
                &candidate_block_id,
                &mint_job_id,
                &reorg_event_id,
            ],
        )?;
        self.pool.metrics.record_pg_insert();
        log::debug!("pg: audit {} — {}", category, message);
        Ok(())
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgAuditStore;
