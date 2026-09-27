//! M-08 GBT coordinator leader fencing via PostgreSQL advisory lock.
//!
//! Spec: `docs/mining/.md` §2.3
//! Lock key: `pg_try_advisory_lock(0x5249474F52)` ("RIGOR").
//!
//! Only the lock holder may call `getblocktemplate` / publish jobs (single-writer).

#[cfg(feature = "postgres")]
use crate::error::{PgError, PgResult};

/// Advisory lock key from (`0x5249474F52`).
pub const GBT_LEADER_LOCK_KEY: i64 = 0x0052_4947_4F52;

/// Session-scoped leader fence. Dropping the guard releases the advisory lock.
#[cfg(feature = "postgres")]
pub struct LeaderFenceGuard {
    client: postgres::Client,
    key: i64,
    released: bool,
}

#[cfg(feature = "postgres")]
impl LeaderFenceGuard {
    pub fn key(&self) -> i64 {
        self.key
    }

    pub fn release(&mut self) -> PgResult<()> {
        if self.released {
            return Ok(());
        }
        self.client
            .execute("SELECT pg_advisory_unlock($1)", &[&self.key])?;
        self.released = true;
        Ok(())
    }
}

#[cfg(feature = "postgres")]
impl Drop for LeaderFenceGuard {
    fn drop(&mut self) {
        let _ = self.release();
    }
}

/// Acquire leader fence on a dedicated connection (session-scoped advisory lock).
///
/// Returns `Ok(Some(guard))` when this process is the single writer,
/// `Ok(None)` when another leader holds the fence.
#[cfg(feature = "postgres")]
pub fn try_acquire_gbt_leader_url(database_url: &str) -> PgResult<Option<LeaderFenceGuard>> {
    let mut client = postgres::Client::connect(database_url, postgres::NoTls)
        .map_err(|e| PgError::Db(e.to_string()))?;
    let row = client.query_one(
        "SELECT pg_try_advisory_lock($1)",
        &[&GBT_LEADER_LOCK_KEY],
    )?;
    let acquired: bool = row.get(0);
    if !acquired {
        return Ok(None);
    }
    Ok(Some(LeaderFenceGuard {
        client,
        key: GBT_LEADER_LOCK_KEY,
        released: false,
    }))
}

/// Probe fencing: first acquires, second blocked, failover after release.
#[cfg(feature = "postgres")]
pub fn fencing_probe(database_url: &str) -> PgResult<FencingProbeReport> {
    let mut a = try_acquire_gbt_leader_url(database_url)?.ok_or_else(|| {
        PgError::NotAvailable("unexpected: first acquire failed on empty cluster".into())
    })?;
    let b = try_acquire_gbt_leader_url(database_url)?;
    let second_blocked = b.is_none();
    a.release()?;
    drop(b);
    let c = try_acquire_gbt_leader_url(database_url)?;
    let failover_ok = c.is_some();
    Ok(FencingProbeReport {
        lock_key: GBT_LEADER_LOCK_KEY,
        first_acquired: true,
        second_blocked,
        failover_after_release: failover_ok,
        single_writer: second_blocked && failover_ok,
    })
}

#[derive(Clone, Debug, PartialEq, Eq, serde::Serialize)]
pub struct FencingProbeReport {
    pub lock_key: i64,
    pub first_acquired: bool,
    pub second_blocked: bool,
    pub failover_after_release: bool,
    pub single_writer: bool,
}

#[cfg(all(test, feature = "postgres"))]
mod tests {
    use super::*;

    #[test]
    #[ignore = "requires DATABASE_URL live PostgreSQL"]
    fn fencing_enforces_single_writer() {
        let url = std::env::var("DATABASE_URL").expect("DATABASE_URL");
        let report = fencing_probe(&url).expect("fencing probe");
        assert!(report.single_writer);
        assert!(report.second_blocked);
        assert!(report.failover_after_release);
    }
}
