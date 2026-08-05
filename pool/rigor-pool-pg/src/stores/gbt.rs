#[cfg(feature = "postgres")]
use serde_json::Value;

#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

/// GBT template archive + longpollid audit.
#[cfg(feature = "postgres")]
pub struct PgGbtStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgGbtStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn archive_template(
        &self,
        record_id: uuid::Uuid,
        tip_hash: &str,
        longpollid: &str,
        gbt_json: &Value,
        payload_hash: &str,
    ) -> PgResult<()> {
        let mut client = self.pool.get()?;
        client.execute(
            "INSERT INTO gbt_templates (record_id, tip_hash, longpollid, gbt_json, payload_hash) \
             VALUES ($1, $2, $3, $4, $5) ON CONFLICT (record_id) DO NOTHING",
            &[&record_id, &tip_hash, &longpollid, &gbt_json, &payload_hash],
        )?;
        self.pool.metrics.record_pg_insert();
        log::info!("pg: archive gbt record_id={record_id} longpollid={longpollid}");
        Ok(())
    }

    pub fn get_last_longpollid(&self, tip: &str) -> PgResult<Option<String>> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT longpollid FROM gbt_templates WHERE tip_hash = $1 ORDER BY ingested_at DESC LIMIT 1",
            &[&tip],
        )?;
        Ok(rows.first().map(|r| r.get::<_, String>(0)))
    }

    pub fn get_gbt_json(&self, record_id: uuid::Uuid) -> PgResult<Option<Value>> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT gbt_json FROM gbt_templates WHERE record_id = $1",
            &[&record_id],
        )?;
        Ok(rows.first().map(|r| r.get(0)))
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgGbtStore;
