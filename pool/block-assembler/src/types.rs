use rigor_gbt::{RecordId, TemplateVersion, TipHash};
use serde::{Deserialize, Serialize};
use std::fmt;

/// Stratum-facing job identifier (monotonic, never recycled).
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct JobId(pub String);

impl fmt::Debug for JobId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "JobId({})", self.0)
    }
}

/// Pool candidate block row id (1:1 with `job_id`).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
pub struct CandidateBlockId(pub u64);

/// JobSnapshot lifecycle (TASK-MINING-007).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum JobState {
    Created,
    Active,
    Stale,
    Invalidated,
    Archived,
}

impl JobState {
    pub fn is_servable(&self) -> bool {
        matches!(self, JobState::Active)
    }
}

/// Coinbase assembly material for Stratum / submitblock reconstruction.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CoinbaseInfo {
    pub coinb1_hex: String,
    pub coinb2_hex: String,
    pub extranonce2_size: u32,
    pub scriptsig_hex: String,
    pub convergence_root_hex: String,
    pub payout_script_hex: String,
    pub coinbase_value: i64,
}

/// CVGE / RIGOR / witness commitment scripts in coinbase.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CommitmentInfo {
    pub cvge_script_hex: Option<String>,
    pub rigor_script_hex: String,
    pub witness_commitment_script_hex: Option<String>,
    pub prev_nbits_hex: String,
}

/// Immutable placeholder reserving vtx[k+1] for future RES body mint (TASK-MINING-007).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BodyMintPlaceholder {
    /// Index in assembled vtx (after GBT transactions).
    pub slot_index: usize,
    pub reserved: bool,
    pub mint_required: bool,
    pub body_mint_txid: Option<String>,
    /// Fixed placeholder txid for merkle pre-image until Mint Engine fills slot.
    pub placeholder_txid: String,
}

/// Published immutable work artifact.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct PublishedJobSnapshot {
    pub job_id: JobId,
    pub candidate_block_id: CandidateBlockId,
    pub record_id: RecordId,
    pub tip_hash: TipHash,
    pub template_version: TemplateVersion,
    pub previousblockhash: String,
    pub height: u64,
    pub state: JobState,
    pub coinbase: CoinbaseInfo,
    pub commitments: CommitmentInfo,
    pub body_mint: BodyMintPlaceholder,
    pub merkle_branch: Vec<String>,
    pub version: u32,
    pub nbits: String,
    pub ntime: u64,
    pub prevhash: String,
    pub block_target: String,
    pub assembly_hash: [u8; 32],
    pub created_at: u64,
    pub state_changed_at: u64,
}
