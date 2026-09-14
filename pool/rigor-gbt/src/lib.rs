//! # rigor-gbt
//!
//! RIGOR pool stack GBT adapter and in-memory template cache (RFC-020 ORU-only).
//!
//! Parses shipping `getblocktemplate` fields from `rigord` and **rejects** leftover
//! lottery keys. Lottery draw types live in [`legacy`] for leftover audit crates only
//! and are not deserialized into shipping [`GbtTemplate`].
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
pub mod legacy;
pub mod record;
pub mod rpc;
pub mod types;
pub mod validate;

pub use adapter::{GbtAdapter, PollEvent};
pub use pg_adapter::GbtPgAdapter;
pub use api::TemplateCacheApi;
pub use cache::TemplateCache;
pub use error::{GbtError, GbtResult};
pub use gbt::{GbtTemplate, GbtTransaction};
pub use record::{build_template_record, normalize_gbt_record, GbtRecord, JobSnapshot, TemplateRecord};
pub use rpc::{BlockchainInfo, RigordRpc};
pub use types::{JobId, RecordId, TemplateState, TemplateVersion, TipHash};
pub use validate::validate_template;
