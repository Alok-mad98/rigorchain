//! # stratum-server
//!
//! RIGOR pool Stratum v1 server and full miner submission pipeline (TASK-MINING-009).
//!
//! Integrates read-only `rigor-gbt`, `block-assembler`, and `share-validator` without
//! modifying consensus or upstream crates.
//!
//! ## Impact
//!
//! - Consensus: **NONE**
//! - Protocol wire: **Stratum v1 unchanged**
//! - Freeze: **NONE**
//! - `equivalence_claimed`: **false**

pub mod audit;
pub mod auth;
pub mod difficulty;
pub mod error;
pub mod jobs;
pub mod metrics;
pub mod orchestrator;
pub mod payout;
pub mod pg_adapter;
pub mod protocol;
pub mod res_mint;
pub mod server;
pub mod session;
pub mod telemetry;
pub mod vardiff;
pub mod winner;
pub mod winner_service;

pub use audit::{AuditEvent, AuditLog, SharedAuditLog};
pub use auth::{
    hash_password, validate_password_strength, verify_password, AuthError,
    HashedCredentialAuthenticator, SharedPasswordAuthenticator, WorkerAuthenticator,
};
pub use error::{StratumError, StratumResult};
pub use jobs::{JobAssignmentEngine, StratumJobEnvelope};
pub use metrics::{MetricsSnapshot, PoolMetrics, SharedMetrics};
pub use mint_engine::submit::{
    BlockSubmitter, RecordingBlockSubmitter, RpcBlockSubmitter, UnconfiguredBlockSubmitter,
};
pub use orchestrator::StratumOrchestrator;
pub use payout::{
    difficulty_to_weight, MinerBalance, PayoutEngine, PayoutLedgerSnapshot, PayoutLine, PayoutRound,
    PayoutRoundStatus, ReverseReport, SharedPayoutEngine, DIFFICULTY_SCALE,
};
pub use pg_adapter::StratumPgAdapter;
pub use protocol::{StratumNotify, StratumRequest, StratumResponse};
pub use res_mint::{BodyMintArtifact, MintEngine, MintEngineConfig};
pub use winner::RES_REWARD_ATOMS;
pub use server::{ServerLimits, StratumServer};
pub use session::{MinerSession, SessionManager, SharedSessionManager};
pub use vardiff::{VardiffConfig, VardiffWindow};
pub use winner::WinnerPreview;
pub use winner_service::SharedWinnerService;
