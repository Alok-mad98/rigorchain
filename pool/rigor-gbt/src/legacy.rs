//! Leftover lottery-draw types for winner-engine / mint-engine compile only.
//!
//! These types are **not** part of shipping [`crate::GbtTemplate`] / [`crate::GbtRecord`].
//! RFC-020 ORU-only mining rejects the corresponding GBT keys at V-06.

use serde::{Deserialize, Serialize};

/// One member of a leftover parent-block convergence set (not shipping GBT).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDrawCandidate {
    /// Display-order txid, as `uint256::GetHex` writes it.
    pub txid: String,
    /// Draw weight f_i — the convergence amount in Tharns.
    pub weight: u64,
    /// The 32-byte witness v1 RES program this convergence declared (A-5 R1).
    pub res_program: String,
    /// σ_i as the node computed it; display order.
    #[serde(default)]
    pub sigma: Option<String>,
    /// Whether σ_i < T·f_i.
    #[serde(default)]
    pub eligible: bool,
}

/// Leftover draw winner. Absent when nobody was eligible.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDrawWinner {
    pub index: u32,
    pub txid: String,
    pub weight: u64,
    pub res_program: String,
}

/// Leftover parent-draw payload. Not deserialized into shipping GBT.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDraw {
    pub source_blockhash: String,
    pub source_height: u64,
    pub sigma_seed: String,
    pub payout_height: u64,
    pub convergence_root: String,
    pub target_prev: String,
    pub res_probability_target: String,
    pub eligible_root: String,
    #[serde(default)]
    pub candidates: Vec<ConvergenceDrawCandidate>,
    #[serde(default)]
    pub winner: Option<ConvergenceDrawWinner>,
}

/// Leftover coinbase OP_RES_MINT description. Not a shipping GBT field.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct GbtResMint {
    pub value: i64,
    #[serde(rename = "scriptPubKey")]
    pub script_pubkey: String,
}
