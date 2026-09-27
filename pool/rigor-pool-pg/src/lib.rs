//! # rigor-pool-pg
//!
//! PostgreSQL persistence infrastructure for the RIGOR Rust pool stack .
//!
//! Domain adapters live in each pool crate's `pg_adapter.rs`; this crate provides
//! connection pooling, schema migration, and SQL store primitives.
//!
//! ## Impact
//!
//! - Consensus: **NONE**
//! - Protocol: **NONE**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod backend;
pub mod error;
pub mod leader_fence;
pub mod metrics;
pub mod pool;
pub mod stores;

pub use backend::PoolBackend;
pub use error::{PgError, PgResult};
pub use leader_fence::{FencingProbeReport, GBT_LEADER_LOCK_KEY};
#[cfg(feature = "postgres")]
pub use leader_fence::{fencing_probe, try_acquire_gbt_leader_url, LeaderFenceGuard};
pub use metrics::{MetricsSnapshot, PersistenceMetrics};
pub use pool::PgPool;
pub use stores::{
    PgAuditStore, PgGbtStore, PgJobSnapshotStore, PgMintStore, PgPayoutStore, PgShareStore,
    PgWorkerStore, RecoveryEngine, RecoveryReport, TreasuryReserveInput,
};

pub const POSTGRES_FEATURE_ENABLED: bool = cfg!(feature = "postgres");
pub const EQUIVALENCE_CLAIMED: bool = false;
