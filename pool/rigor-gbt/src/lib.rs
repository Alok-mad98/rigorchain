//! # rigor-gbt
//!
//! RIGOR pool stack GBT adapter and in-memory template cache (TASK-MINING-006).
//!
//! Implements `docs/mining/RIGOR_GBT_ADAPTER_SPEC.md` (TASK-MINING-004) and
//! `docs/mining/TEMPLATE_CACHE_AND_ASSEMBLER_SPEC.md` (TASK-MINING-005) within
//! infrastructure scope: no Stratum, Winner Engine, RES Mint, or persistence layer.
//!
//! ## Architecture impact
//!
//! - Consensus: **NONE**
//! - Protocol: **NONE**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod adapter;
pub mod pg_adapter;
pub mod api;
pub mod cache;
pub mod error;
pub mod gbt;
pub mod record;
pub mod rpc;
pub mod types;
pub mod validate;

pub use adapter::{GbtAdapter, PollEvent};
pub use pg_adapter::GbtPgAdapter;
pub use api::TemplateCacheApi;
pub use cache::TemplateCache;
pub use error::{GbtError, GbtResult};
pub use gbt::{
    ConvergenceDraw, ConvergenceDrawCandidate, ConvergenceDrawWinner, GbtResMint, GbtTemplate,
    GbtTransaction,
};
pub use record::{build_template_record, normalize_gbt_record, GbtRecord, JobSnapshot, TemplateRecord};
pub use rpc::{BlockchainInfo, RigordRpc};
pub use types::{JobId, RecordId, TemplateState, TemplateVersion, TipHash};
pub use validate::validate_template;
