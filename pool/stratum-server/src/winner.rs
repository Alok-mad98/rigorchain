//! Leftover lottery preview types. Not on the RFC-020 mining path.
//!
//! The orchestrator does not preview winners. This module exists only so older
//! telemetry / docs references keep a name.

/// Frozen RES reward leftover constant. Unused on the ORU-only path.
pub const RES_REWARD_ATOMS: i64 = 10_000;

/// Leftover operator-telemetry shape. Never populated by the mining path.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WinnerPreview {
    pub mint_required: bool,
    pub winner_txid: Option<String>,
    pub convergence_root_hex: String,
    pub draw_target_prev_nbits: String,
}
