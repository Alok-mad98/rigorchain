//! # winner-engine
//!
//! Leftover lottery audit crate. **Not on the RFC-020 mining path.**
//! Shipping pool (rigor-gbt → block-assembler → share-validator → stratum / pilot)
//! does not construct or depend on this crate.
//!
//! ## Architecture impact
//!
//! - Consensus: **NONE**
//! - Pool production path: **NONE**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod authorize;
pub mod candidate;
pub mod constants;
pub mod draw;
pub mod engine;
pub mod error;
pub mod hash;
pub mod math;
pub mod persist;
pub mod pg_adapter;
pub mod service;
pub mod telemetry;
pub mod reorg;
pub mod root;
pub mod sigma;
pub mod types;

pub use authorize::{
    authorize_mint, replay_draw_from_candidate_set, validate_authorization,
    validate_token_consumption,
};
pub use candidate::{extract_parent_draw_candidates, validate_convergence_membership};
pub use constants::{
    CONVERGENCE_DRAW_MIN_TXS_FOR_RES, EQUIVALENCE_CLAIMED, GOLDEN_PREV_NBITS, REPLAY_CERT_TAG,
    RES_PROBABILITY_TARGET,
};
pub use sigma::compute_sigma_hash;
pub use math::convergence_draw_inequality_holds;
pub use root::compute_convergence_root_with_encoding;
pub use draw::{
    draw_input_from_golden, replay_convergence_draw, verify_replay_against_node, DrawReplayInput,
};
pub use engine::WinnerEngine;
pub use error::{WinnerError, WinnerResult};
pub use hash::{
    display_hex_to_native, hash256, hash_hex_to_native, native_to_display_hex, parse_prev_nbits,
    uint256_cmp,
};
pub use persist::{
    CandidateRepository, InMemoryWinnerStore, MintAuthorizationRepository, WinnerRepository,
};
#[cfg(feature = "postgres")]
pub use persist::PgWinnerStore;
pub use reorg::{
    check_winner_mismatch, invalidate_winner, orphan_candidates_on_reorg, revoke_authorization,
    revoke_tokens_for_jobs, RevocationReason,
};
pub use root::{
    compute_convergence_root, compute_convergence_root_for_rigord_f,
    compute_convergence_root_from_native, native_to_hash_hex, TxidEncoding,
};
pub use pg_adapter::{get_winner_by_candidate, persist_golden_draw, WinnerPgBackend};
pub use service::{WinnerPreviewResult, WinnerRuntime};
pub use types::*;
