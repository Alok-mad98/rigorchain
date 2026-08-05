use std::sync::{Arc, OnceLock};

use block_assembler::PublishedJobSnapshot;
use rigor_pool_metrics::{
    now_record_timestamps, JsonlMetricsWriter, SCHEMA_VERSION,
};
use serde::Serialize;

use crate::reorg::RevocationReason;
use crate::types::DrawReplayResult;

const WINNER_FILE: &str = "winner_authorization.jsonl";

fn global_writer() -> Option<Arc<JsonlMetricsWriter>> {
    static WRITER: OnceLock<Option<Arc<JsonlMetricsWriter>>> = OnceLock::new();
    WRITER
        .get_or_init(|| {
            JsonlMetricsWriter::open_default(WINNER_FILE)
                .ok()
                .map(Arc::new)
        })
        .clone()
}

#[derive(Clone, Copy, Debug)]
pub enum WinnerAuthEvent {
    Preview,
    Authorize,
    Invalidate,
    Revoke,
}

impl WinnerAuthEvent {
    fn as_str(self) -> &'static str {
        match self {
            Self::Preview => "preview",
            Self::Authorize => "authorize",
            Self::Invalidate => "invalidate",
            Self::Revoke => "revoke",
        }
    }
}

pub struct WinnerTelemetry;

impl WinnerTelemetry {
    pub fn record_preview(job: &PublishedJobSnapshot, draw: &DrawReplayResult) {
        Self::record(
            WinnerAuthEvent::Preview,
            job,
            draw,
            false,
            None,
            None,
        );
    }

    pub fn record_authorize(job: &PublishedJobSnapshot, draw: &DrawReplayResult) {
        Self::record(
            WinnerAuthEvent::Authorize,
            job,
            draw,
            true,
            None,
            None,
        );
    }

    pub fn record_invalidate(
        job: &PublishedJobSnapshot,
        draw: &DrawReplayResult,
        reason: &RevocationReason,
    ) {
        Self::record(
            WinnerAuthEvent::Invalidate,
            job,
            draw,
            false,
            Some(format!("{reason:?}")),
            None,
        );
    }

    pub fn record_revoke(
        job: &PublishedJobSnapshot,
        draw: &DrawReplayResult,
        reason: &RevocationReason,
    ) {
        Self::record(
            WinnerAuthEvent::Revoke,
            job,
            draw,
            false,
            Some(format!("{reason:?}")),
            None,
        );
    }

    /// Operational preview when draw replay is unavailable (e.g. devtest FVR-M gap).
    pub fn record_preview_degraded(
        job: &PublishedJobSnapshot,
        template: &rigor_gbt::TemplateRecord,
        degradation_reason: &str,
    ) {
        use crate::types::{DrawReplayResult, ValidationMethod};
        let draw = DrawReplayResult {
            convergence_root: [0u8; 32],
            convergence_root_display: job.coinbase.convergence_root_hex.clone(),
            draw_winner_txid: None,
            draw_winner_txid_display: Some("0".repeat(64)),
            winner_index: -1,
            eligible_count: template.gbt.convergence_txids.len() as u32,
            mint_required: false,
            candidates: vec![],
            validation_method: ValidationMethod::Replay,
        };
        Self::record(
            WinnerAuthEvent::Preview,
            job,
            &draw,
            false,
            Some(format!("degraded:{degradation_reason}")),
            None,
        );
    }

    fn record(
        event: WinnerAuthEvent,
        job: &PublishedJobSnapshot,
        draw: &DrawReplayResult,
        authorized: bool,
        reason: Option<String>,
        authorization_token_id: Option<String>,
    ) {
        let Some(writer) = global_writer() else {
            return;
        };
        let ts = now_record_timestamps();
        let row = WinnerAuthRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            event: event.as_str().to_string(),
            job_id: job.job_id.0.clone(),
            candidate_block_id: Some(job.candidate_block_id.0),
            node_height: job.height,
            tip_hash: job.tip_hash.0.clone(),
            f_set_size: draw.eligible_count as u64,
            draw_winner_txid: draw
                .draw_winner_txid_display
                .clone()
                .unwrap_or_else(|| "0".repeat(64)),
            sigma_digest_hex: sigma_for_draw(draw),
            convergence_root_hex: draw.convergence_root_display.clone(),
            bind_hash_hex: None,
            mint_required: draw.mint_required,
            authorization_token_id,
            revocation_reason: reason.clone(),
            authorized: Some(authorized),
            reason,
        };
        let _ = writer.append_json_line(&row);
        let _ = writer.flush();
    }
}

#[derive(Serialize)]
struct WinnerAuthRow {
    schema_version: &'static str,
    ts: String,
    event: String,
    job_id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    candidate_block_id: Option<u64>,
    node_height: u64,
    tip_hash: String,
    f_set_size: u64,
    draw_winner_txid: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    sigma_digest_hex: Option<String>,
    convergence_root_hex: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    bind_hash_hex: Option<String>,
    mint_required: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    authorization_token_id: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    revocation_reason: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    authorized: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    reason: Option<String>,
}

fn sigma_for_draw(draw: &DrawReplayResult) -> Option<String> {
    if draw.winner_index >= 0 {
        if let Some(c) = draw.candidates.get(draw.winner_index as usize) {
            return Some(c.h_display.clone());
        }
    }
    draw.candidates
        .iter()
        .find(|c| c.draw_eligible)
        .map(|c| c.h_display.clone())
}
