//! # share-validator
//!
//! RIGOR pool stack share and block validation layer (TASK-MINING-008).
//!
//! Enforcement boundary between miner submissions, Block Assembler jobs, and
//! `rigord` consensus validation. Read-only integration with `rigor-gbt` and
//! `block-assembler`.
//!
//! ## Impact
//!
//! - Consensus: **NONE** (client-side verifier; no `rigord` changes)
//! - Protocol: **NONE**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod commitments;
pub mod convergence;
pub mod error;
pub mod header;
pub mod job_binding;
pub mod opuhash;
pub mod pg_adapter;
pub mod replay;
pub mod res_mint;
pub mod txwire;
pub mod types;
pub mod validator;

pub use error::{ValidationErrorCode, ValidatorError, ValidatorResult};
pub use pg_adapter::SharePgAdapter;
pub use opuhash::{
    expand_compact, expand_compact_bits, get_opus_hash_from_base_hash, verify_opuhash_block,
    verify_opuhash_share, OPUS_POW_BUFFER_SIZE, OPUS_POW_MIX_ROUNDS,
};
pub use replay::{ReplayGuard, SharedReplayGuard};
pub use types::{
    BlockSubmission, JobValidationState, ShareSubmission, ValidationKind, ValidationResult,
};
pub use convergence::{CommittedWindowSource, ParentWindow, WindowCandidate};
pub use job_binding::validate_job_template_binding;
pub use res_mint::{validate_node_supplied_automint, RES_REWARD_ATOMS};
pub use validator::ShareValidator;
