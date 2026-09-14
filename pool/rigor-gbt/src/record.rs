use crate::gbt::{GbtTemplate, GbtTransaction};
use crate::types::{JobId, RecordId, TemplateState, TemplateVersion, TipHash};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

/// Immutable adapter snapshot (TASK-MINING-004). RFC-020 shipping fields only.
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
    pub default_witness_commitment: Option<String>,
    pub weightlimit: Option<u64>,
    pub version: Option<u32>,
    pub gbt_payload_hash: [u8; 32],
    pub raw_json: String,
    #[serde(default)]
    pub default_coinbase_scriptsig: Option<String>,
    #[serde(default)]
    pub default_coinbase_tx: Option<String>,
    #[serde(default)]
    pub default_coinbase_txid: Option<String>,
    #[serde(default)]
    pub merkleroot: Option<String>,
    #[serde(default)]
    pub rigor_coinbase_immutable: Option<bool>,
    #[serde(default)]
    pub coinbase_address: Option<String>,
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
        default_witness_commitment: template.default_witness_commitment,
        weightlimit: template.weightlimit,
        version: template.version,
        gbt_payload_hash: payload_hash,
        raw_json,
        default_coinbase_scriptsig: template.default_coinbase_scriptsig,
        default_coinbase_tx: template.default_coinbase_tx,
        default_coinbase_txid: template.default_coinbase_txid,
        merkleroot: template.merkleroot,
        rigor_coinbase_immutable: template.rigor_coinbase_immutable,
        coinbase_address: template.coinbase_address,
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
        job_id: None,
        candidate_block_id: None,
        created_at: now,
        state_changed_at: now,
        gbt,
        job_snapshot: None,
    }
}
