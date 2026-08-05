//! # block-assembler
//!
//! RIGOR pool stack block assembler and JobSnapshot publisher (TASK-MINING-007).
//!
//! Consumes `TemplateRecord` from [`rigor_gbt`], assembles coinbase (height‖C),
//! witness / CVGE / RIGOR commitments, reserves a body-mint placeholder, and
//! publishes immutable `JobSnapshot` rows with monotonic `job_id` /
//! `candidate_block_id` pairs.
//!
//! ## Impact
//!
//! - Consensus: **NONE**
//! - Protocol: **NONE**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod api;
pub mod pg_adapter;
pub mod assembly;
pub mod cache;
pub mod coinbase;
pub mod convergence;
pub mod error;
pub mod merkle;
pub mod payout_script;
pub mod placeholder;
pub mod publisher;
pub mod script;
pub mod types;

pub use api::JobSnapshotApi;
pub use pg_adapter::AssemblerPgAdapter;
pub use assembly::{
    assemble_from_gbt, assemble_from_template, AssemblerConfig, AssemblyArtifact,
    UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT,
};
pub use cache::JobCache;
pub use coinbase::{assemble_coinbase, CoinbaseAssembly, CoinbaseConfig};
pub use convergence::{
    compute_convergence_root, compute_convergence_root_for_rigord_f,
    compute_convergence_root_from_native, hash256, native_to_hash_hex,
};
pub use error::{AssemblerError, AssemblerResult};
pub use payout_script::{
    is_unspendable_placeholder, payout_script_from_hex, validate_payout_script, PayoutScriptKind,
};
pub use placeholder::{body_mint_placeholder_txid, build_body_mint_placeholder};
pub use publisher::JobPublisher;
pub use types::{
    BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    PublishedJobSnapshot,
};
