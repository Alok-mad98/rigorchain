use std::sync::{Arc, OnceLock};

use block_assembler::PublishedJobSnapshot;
use rigor_pool_metrics::{
    now_record_timestamps, JsonlMetricsWriter, SCHEMA_VERSION,
};
use serde::Serialize;

use crate::types::{MintRecord, MintState};

const MINT_FILE: &str = "mint_engine.jsonl";

fn global_writer() -> Option<Arc<JsonlMetricsWriter>> {
    static WRITER: OnceLock<Option<Arc<JsonlMetricsWriter>>> = OnceLock::new();
    WRITER
        .get_or_init(|| {
            JsonlMetricsWriter::open_default(MINT_FILE)
                .ok()
                .map(Arc::new)
        })
        .clone()
}

pub struct MintTelemetry;

impl MintTelemetry {
    pub fn record_mint_started(job: &PublishedJobSnapshot, winner_txid: &str) {
        Self::append("mint_started", job, winner_txid, None, "started", None, None);
    }

    /// M-06 bind_hash preflight — chain-accurate ConvergenceDrawBindHashFromParent check.
    pub fn record_bind_hash_preflight(
        job: &PublishedJobSnapshot,
        bind_hash_hex: &str,
        bind_height: i64,
        source: &str,
        matched_parent_fields: bool,
    ) {
        let Some(writer) = global_writer() else {
            return;
        };
        let ts = now_record_timestamps();
        let row = BindHashPreflightRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            event: "bind_hash_preflight".to_string(),
            job_id: job.job_id.0.clone(),
            node_height: job.height,
            bind_hash_hex: bind_hash_hex.to_string(),
            bind_height,
            source: source.to_string(),
            matched_parent_fields,
            state: if matched_parent_fields {
                "pass".to_string()
            } else {
                "fail".to_string()
            },
        };
        let _ = writer.append_json_line(&row);
        let _ = writer.flush();
    }

    /// M-07 mempool preflight — testmempoolaccept before submitblock.
    pub fn record_mempool_preflight(
        job: &PublishedJobSnapshot,
        mint_txid: &str,
        allowed: bool,
        reject_reason: Option<&str>,
    ) {
        let Some(writer) = global_writer() else {
            return;
        };
        let ts = now_record_timestamps();
        let row = MempoolPreflightRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            event: "mempool_preflight".to_string(),
            job_id: job.job_id.0.clone(),
            node_height: job.height,
            mint_txid: mint_txid.to_string(),
            allowed,
            reject_reason: reject_reason.map(|s| s.to_string()),
            state: if allowed {
                "pass".to_string()
            } else {
                "reject".to_string()
            },
        };
        let _ = writer.append_json_line(&row);
        let _ = writer.flush();
    }

    pub fn record_mint_built(job: &PublishedJobSnapshot, winner_txid: &str, mint_txid: &str) {
        Self::append(
            "mint_built",
            job,
            winner_txid,
            Some(mint_txid),
            "built",
            None,
            None,
        );
    }

    pub fn record_mint_submitted(job: &PublishedJobSnapshot, record: &MintRecord) {
        Self::append(
            "mint_submitted",
            job,
            &record.mint_job.winner_txid,
            Some(&record.artifact.body_mint_txid),
            "submitted",
            Some(record.artifact.amount_atoms as u64),
            None,
        );
    }

    pub fn record_mint_revoked(record: &MintRecord) {
        Self::append_fields(
            "mint_revoked",
            &record.mint_job.job_id,
            0,
            &record.mint_job.winner_txid,
            Some(&record.artifact.body_mint_txid),
            "revoked",
            Some(record.artifact.amount_atoms as u64),
            None,
        );
    }

    pub fn record_mint_confirmed(
        job: &PublishedJobSnapshot,
        record: &MintRecord,
        confirmations: u32,
    ) {
        Self::append(
            "mint_confirmed",
            job,
            &record.mint_job.winner_txid,
            Some(&record.artifact.body_mint_txid),
            "confirmed",
            Some(record.artifact.amount_atoms as u64),
            Some(confirmations),
        );
    }

    /// Job publish observation when RES mint path is not taken (TASK-MINING-025 devtest).
    pub fn record_publish_observation(
        job: &PublishedJobSnapshot,
        mint_required: bool,
        winner_txid: Option<&str>,
    ) {
        if mint_required {
            return;
        }
        let zero_winner = "0".repeat(64);
        let winner = winner_txid.unwrap_or(&zero_winner);
        Self::append_fields(
            "mint_failed",
            &job.job_id.0,
            job.height,
            winner,
            None,
            "not_required",
            None,
            None,
        );
    }

    fn append(
        event: &'static str,
        job: &PublishedJobSnapshot,
        winner_txid: &str,
        mint_txid: Option<&str>,
        state: &str,
        mint_amount_atoms: Option<u64>,
        confirmations: Option<u32>,
    ) {
        Self::append_fields(
            event,
            &job.job_id.0,
            job.height,
            winner_txid,
            mint_txid,
            state,
            mint_amount_atoms,
            confirmations,
        );
    }

    fn append_fields(
        event: &'static str,
        job_id: &str,
        node_height: u64,
        winner_txid: &str,
        mint_txid: Option<&str>,
        state: &str,
        mint_amount_atoms: Option<u64>,
        confirmations: Option<u32>,
    ) {
        let Some(writer) = global_writer() else {
            return;
        };
        let ts = now_record_timestamps();
        let row = MintEventRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            event: event.to_string(),
            job_id: job_id.to_string(),
            node_height,
            draw_winner_txid: winner_txid.to_string(),
            mint_txid: mint_txid.map(|s| s.to_string()),
            mint_vout: Some(0),
            mint_amount_atoms,
            confirmations,
            state: state.to_string(),
            confirmation_height: None,
        };
        let _ = writer.append_json_line(&row);
        let _ = writer.flush();
    }

    pub fn state_label(state: MintState) -> &'static str {
        match state {
            MintState::Pending => "pending",
            MintState::Built => "built",
            MintState::Submitted => "submitted",
            MintState::Confirmed => "confirmed",
            MintState::Revoked => "revoked",
            MintState::Orphaned => "orphaned",
        }
    }
}

#[derive(Serialize)]
struct MintEventRow {
    schema_version: &'static str,
    ts: String,
    event: String,
    job_id: String,
    node_height: u64,
    draw_winner_txid: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    mint_txid: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    mint_vout: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    mint_amount_atoms: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    confirmations: Option<u32>,
    state: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    confirmation_height: Option<u64>,
}

#[derive(Serialize)]
struct BindHashPreflightRow {
    schema_version: &'static str,
    ts: String,
    event: String,
    job_id: String,
    node_height: u64,
    bind_hash_hex: String,
    bind_height: i64,
    source: String,
    matched_parent_fields: bool,
    state: String,
}

#[derive(Serialize)]
struct MempoolPreflightRow {
    schema_version: &'static str,
    ts: String,
    event: String,
    job_id: String,
    node_height: u64,
    mint_txid: String,
    allowed: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    reject_reason: Option<String>,
    state: String,
}
