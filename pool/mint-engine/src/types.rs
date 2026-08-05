use rigor_gbt::RecordId;
use serde::{Deserialize, Serialize};

/// Frozen: 100.00 RES = 100 * RES_COIN = 10000 atoms.
pub const RES_REWARD_ATOMS: i64 = 10_000;
pub const MIN_CONVERGENCE_AMOUNT_SAT: u64 = 100_000_000;

/// Winner preview inputs (mirrors stratum-server `WinnerPreview` without coupling).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MintWinnerContext {
    pub mint_required: bool,
    pub winner_txid: Option<String>,
    pub convergence_root_hex: String,
    pub draw_target_prev_nbits: String,
    /// Optional authorization-token bind (native). Used when RPC is unavailable.
    pub authorized_bind_hash: Option<[u8; 32]>,
    /// Optional authorization-token bind (display hex).
    pub authorized_bind_hash_display: Option<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub struct MintJobRef {
    pub winner_job_id: Option<String>,
    pub job_id: String,
    pub record_id: RecordId,
    pub winner_txid: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ConvergeProofFields {
    pub convergence_txid: [u8; 32],
    pub convergence_amount_sat: u64,
    pub bind_hash: [u8; 32],
    pub height: u64,
    pub target_prev: u32,
    pub convergence_root: [u8; 32],
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct OruFundingInput {
    pub prev_txid: [u8; 32],
    pub vout: u32,
    pub amount: i64,
    pub script_pubkey: Vec<u8>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MintBuildParams {
    pub mint_job: MintJobRef,
    pub proof: ConvergeProofFields,
    pub res_payout_script: Vec<u8>,
    pub oru_change_script: Vec<u8>,
    pub funding: OruFundingInput,
    pub oru_fee: i64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BodyMintArtifact {
    pub body_mint_txid: String,
    pub raw_tx_hex: String,
    pub amount_atoms: i64,
    pub winner_txid: String,
    pub witness_item_count: usize,
    pub sigma_digest_hex: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CandidateBlockPackage {
    pub block_hex: String,
    pub body_mint_txid: String,
    pub vtx_count: usize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum MintState {
    Pending,
    Built,
    Submitted,
    Confirmed,
    Orphaned,
    Revoked,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MintRecord {
    pub mint_job: MintJobRef,
    pub state: MintState,
    pub artifact: BodyMintArtifact,
    pub created_at: u64,
    pub state_changed_at: u64,
}
