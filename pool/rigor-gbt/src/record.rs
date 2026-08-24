use crate::gbt::{ConvergenceDraw, GbtTemplate, GbtTransaction};
use crate::types::{JobId, RecordId, TemplateState, TemplateVersion, TipHash};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

/// Immutable adapter snapshot (TASK-MINING-004).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct GbtRecord {
    pub record_id: RecordId,
    pub fetched_at: u64,
    pub tip_hash: TipHash,
    pub previousblockhash: String,
    pub height: u64,
    pub longpollid: String,
    pub bits: String,
    pub prev_nbits: String,
    pub target: Option<String>,
    pub mintime: Option<u64>,
    pub curtime: Option<u64>,
    pub coinbasevalue: i64,
    pub transactions: Vec<GbtTransaction>,
    /// Set this template commits; the NEXT block draws it. A-9-C: spans the whole draw window,
    /// so most members are transactions of earlier blocks and are absent from `transactions`.
    pub convergence_txids: Vec<String>,
    /// C over `convergence_txids`, as the node published it. `None` for pre-A9C records.
    #[serde(default)]
    pub convergence_root: Option<String>,
    /// Resolved parent draw this template pays (SECURITY-001-A8).
    pub convergence_draw: Option<ConvergenceDraw>,
    pub default_witness_commitment: Option<String>,
    pub default_convergence_commitment: Option<String>,
    #[serde(default)]
    pub default_res_mint: Option<crate::gbt::GbtResMint>,
    #[serde(default)]
    pub default_pcorb_carrier: Option<String>,
    pub weightlimit: Option<u64>,
    pub version: Option<u32>,
    pub gbt_payload_hash: [u8; 32],
    pub raw_json: String,
}

/// Published work artifact (TASK-MINING-005 §2.5). Populated when Block Assembler publishes.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct JobSnapshot {
    pub job_id: Option<JobId>,
    pub record_id: RecordId,
    pub coinb1_hex: Option<String>,
    pub coinb2_hex: Option<String>,
    pub merkle_branch: Vec<String>,
    pub version: Option<u32>,
    pub nbits: Option<String>,
    pub ntime: Option<u64>,
    pub prevhash: Option<String>,
    pub share_diff: Option<f64>,
    pub block_target: Option<String>,
    pub mint_required: Option<bool>,
    pub convergence_root: Option<String>,
    pub body_mint_txid: Option<String>,
    pub assembly_hash: Option<[u8; 32]>,
}

/// Canonical cache row (TASK-MINING-005 §2.1).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct TemplateRecord {
    pub record_id: RecordId,
    pub parent_record_id: Option<RecordId>,
    pub template_version: TemplateVersion,
    pub tip_hash: TipHash,
    pub previousblockhash: String,
    pub longpollid: String,
    pub height: u64,
    pub template_state: TemplateState,
    pub gbt_payload_hash: [u8; 32],
    pub mint_required: Option<bool>,
    pub winner_job_id: Option<String>,
    pub job_id: Option<JobId>,
    pub candidate_block_id: Option<String>,
    pub created_at: u64,
    pub state_changed_at: u64,
    pub gbt: GbtRecord,
    pub job_snapshot: Option<JobSnapshot>,
}

pub fn unix_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

pub fn normalize_gbt_record(
    template: GbtTemplate,
    raw_json: String,
    tip_hash: TipHash,
    payload_hash: [u8; 32],
) -> GbtRecord {
    GbtRecord {
        record_id: RecordId::new_v4(),
        fetched_at: unix_now(),
        tip_hash,
        previousblockhash: template.previousblockhash.to_ascii_lowercase(),
        height: template.height,
        longpollid: template.longpollid.clone(),
        bits: template.bits.to_ascii_lowercase(),
        prev_nbits: template.prev_nbits.to_ascii_lowercase(),
        target: template.target.clone(),
        mintime: template.mintime,
        curtime: template.curtime,
        coinbasevalue: template.coinbasevalue,
        transactions: template.transactions,
        convergence_txids: template.convergence_txids,
        convergence_root: template.convergence_root,
        convergence_draw: template.convergence_draw,
        default_witness_commitment: template.default_witness_commitment,
        default_convergence_commitment: template.default_convergence_commitment,
        default_res_mint: template.default_res_mint,
        default_pcorb_carrier: template.default_pcorb_carrier,
        weightlimit: template.weightlimit,
        version: template.version,
        gbt_payload_hash: payload_hash,
        raw_json,
    }
}

pub fn build_template_record(
    gbt: GbtRecord,
    parent_record_id: Option<RecordId>,
    template_version: TemplateVersion,
    template_state: TemplateState,
) -> TemplateRecord {
    let now = unix_now();
    let record_id = gbt.record_id;
    TemplateRecord {
        record_id,
        parent_record_id,
        template_version,
        tip_hash: gbt.tip_hash.clone(),
        previousblockhash: gbt.previousblockhash.clone(),
        longpollid: gbt.longpollid.clone(),
        height: gbt.height,
        template_state,
        gbt_payload_hash: gbt.gbt_payload_hash,
        mint_required: None,
        winner_job_id: None,
        job_id: None,
        candidate_block_id: None,
        created_at: now,
        state_changed_at: now,
        gbt,
        job_snapshot: None,
    }
}
