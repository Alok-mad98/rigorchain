use serde::{Deserialize, Serialize};
use uuid::Uuid;

use rigor_gbt::RecordId;

/// The convergence set F the parent block committed, in the node's enumeration order.
///
/// SECURITY-001-A8: this is the set the block being assembled PAYS, not the set it commits.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct CandidateSet {
    pub txids_ordered: Vec<[u8; 32]>,
    pub txids_display: Vec<String>,
    pub weights: Vec<u64>,
    /// A-5 owner declarations, parallel to `txids_ordered`.
    pub res_programs: Vec<[u8; 32]>,
    pub source_template_id: RecordId,
    pub parent_block_hash_display: String,
    /// B_{h-1} in native byte order — the σ seed.
    pub sigma_seed: [u8; 32],
    /// C over this set, as the node reported it, in native byte order.
    pub convergence_root: [u8; 32],
    /// Compact nBits of the parent header; a mint proof must report it verbatim.
    pub target_prev: u32,
    /// Height of the block that committed the set, h-1.
    pub source_height: u64,
    /// Payout height h — the height fed to σ and the height of the block being assembled.
    pub height: u64,
    pub extracted_at: u64,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct CandidateAudit {
    pub txid: [u8; 32],
    pub txid_display: String,
    pub convergence_amount: u64,
    pub h: [u8; 32],
    pub h_display: String,
    pub draw_eligible: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ValidationMethod {
    Replay,
    GcrPostConnect,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct DrawReplayResult {
    pub convergence_root: [u8; 32],
    pub convergence_root_display: String,
    pub draw_winner_txid: Option<[u8; 32]>,
    pub draw_winner_txid_display: Option<String>,
    pub winner_index: i32,
    pub eligible_count: u32,
    pub mint_required: bool,
    pub candidates: Vec<CandidateAudit>,
    pub validation_method: ValidationMethod,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum WinnerJobStatus {
    Provisional,
    Validated,
    Authorized,
    Confirmed,
    Stale,
    Forbidden,
    Revoked,
    ConfirmationPending,
    ConfirmationDrift,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MintAuthorizationToken {
    pub winner_job_id: i64,
    pub candidate_block_id: i64,
    pub draw_winner_txid: [u8; 32],
    pub draw_winner_txid_display: String,
    /// C over the parent's set — the value the PCORB witness must carry.
    pub convergence_root: [u8; 32],
    pub convergence_root_display: String,
    /// PCORB `bind_hash`: the parent block hash B_{h-1}, verbatim.
    pub bind_hash: [u8; 32],
    pub bind_hash_display: String,
    pub target_prev: u32,
    /// Payout height h.
    pub height: u32,
    pub winner_convergence_amount: u64,
    /// A-5 R2: the mint must pay exactly `OP_1 <res_payout_program>`, the program the winning
    /// convergence declared in its own OP_RETURN. Never a pool-configured address.
    pub res_payout_program: [u8; 32],
    pub res_probability_target: [u8; 32],
    pub authorized_at: u64,
    pub expires_at: u64,
    pub token_nonce: Uuid,
    pub replay_cert_tag: String,
    pub status: WinnerJobStatus,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct WinnerJobRecord {
    pub id: i64,
    pub candidate_block_id: i64,
    pub status: WinnerJobStatus,
    pub mint_required: bool,
    pub eligible_count: u32,
    pub convergence_root_display: String,
    pub winner_txid_display: Option<String>,
    pub preview_audit: DrawReplayResult,
    pub token_nonce: Option<Uuid>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum CandidateBlockStatus {
    Active,
    Found,
    Orphaned,
    Expired,
    Submitted,
    Rejected,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct CandidateBlockRecord {
    pub id: i64,
    pub job_id: String,
    pub record_id: RecordId,
    pub prev_block_hash: String,
    pub tip_hash: String,
    pub height: u64,
    pub convergence_root_display: String,
    pub mint_required: bool,
    pub status: CandidateBlockStatus,
    pub body_mint_reserved: bool,
}

/// Context required for mint authorization (AUTH-01..AUTH-15).
#[derive(Clone, Debug)]
pub struct AuthorizationContext {
    pub draw: DrawReplayResult,
    pub candidate_set: CandidateSet,
    pub candidate_block: CandidateBlockRecord,
    /// C over the parent's set as the node reported it, native bytes. Not the root this
    /// block's own coinbase commits — those are different sets.
    pub node_draw_convergence_root: [u8; 32],
    pub template_bits: String,
    pub template_prev_nbits: String,
    pub current_tip: String,
    pub paused_new_jobs: bool,
    pub treasury_available: bool,
    pub now_unix: u64,
    pub consumed_token_nonces: Vec<Uuid>,
}
