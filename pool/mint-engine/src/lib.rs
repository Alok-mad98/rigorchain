//! # mint-engine (LEGACY)
//!
//! Leftover lottery audit crate. **Not on the RFC-020 mining path.**
//! Historical RES body mint builder; production Stratum / pilot do not depend
//! on this crate. Retained for audit of the former builder only.
//!
//! ## Impact
//!
//! - Consensus: **NONE**
//! - Pool production path: **NONE** (detached)
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod block;
pub mod engine;
pub mod pg_adapter;
pub mod error;
pub mod mempool_preflight;
pub mod res_owner;
pub mod rpc;
pub mod sigma;
pub mod state;
pub mod submit;
pub mod telemetry;
pub mod txbuild;
pub mod types;
pub mod witness;

pub use block::{assemble_block_without_mint, assemble_candidate_block, MintBlockContext};
#[allow(deprecated)]
pub use engine::{
    derive_bind_hash_chain_accurate, derive_bind_hash_infra, MintEngine, MintEngineConfig,
    MintOutcome, CONVERGENCE_DRAW_BIND_DEPTH, DEVTEST_ORU_FUNDING_TXID,
};
pub use pg_adapter::MintPgAdapter;
pub use error::{MintError, MintResult};
pub use mempool_preflight::{
    enforce_mempool_preflight, parse_testmempoolaccept_result, MempoolAcceptResult,
};
pub use rpc::{RigordRpc, RigordRpcConfig};
pub use state::MintStateStore;
pub use submit::{
    BlockSubmitter, RecordingBlockSubmitter, RpcBlockSubmitter, UnconfiguredBlockSubmitter,
};
pub use types::{
    BodyMintArtifact, CandidateBlockPackage, ConvergeProofFields, MintJobRef, MintRecord,
    MintState, MintWinnerContext, OruFundingInput, RES_REWARD_ATOMS,
};
pub use witness::{build_pcorb_witness_stack, PCORB_WITNESS_ITEMS};
pub use winner_engine::MintAuthorizationToken;
