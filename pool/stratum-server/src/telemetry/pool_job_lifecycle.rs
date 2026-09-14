use std::sync::{Arc, OnceLock};

use block_assembler::{JobState, PublishedJobSnapshot};
use rigor_gbt::TemplateRecord;
use rigor_pool_metrics::{
    now_record_timestamps, JsonlMetricsWriter, SCHEMA_VERSION,
};
use serde::Serialize;

const POOL_JOB_FILE: &str = "pool_job_lifecycle.jsonl";

fn global_writer() -> Option<Arc<JsonlMetricsWriter>> {
    static WRITER: OnceLock<Option<Arc<JsonlMetricsWriter>>> = OnceLock::new();
    WRITER
        .get_or_init(|| {
            JsonlMetricsWriter::open_default(POOL_JOB_FILE)
                .ok()
                .map(Arc::new)
        })
        .clone()
}

#[derive(Clone, Default)]
pub struct PoolJobTelemetry;

impl PoolJobTelemetry {
    pub fn record_job_published(
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
        clean_jobs: bool,
        winner_preview_txid: Option<&str>,
        mint_required: bool,
    ) {
        Self::append(PoolJobRow {
            event: "job_published",
            job,
            template,
            clean_jobs: Some(clean_jobs),
            winner_preview_txid: winner_preview_txid.map(|s| s.to_string()),
            mint_required: Some(mint_required),
            retired_at: None,
            job_state: job_state_str(&job.state),
        });
    }

    pub fn record_job_stale(job: &PublishedJobSnapshot, template: Option<&TemplateRecord>) {
        Self::append_with_optional_template(
            "job_stale",
            job,
            template,
            None,
            None,
            Some(now_record_timestamps().ts),
            "STALE",
        );
    }

    pub fn record_job_invalidated(job: &PublishedJobSnapshot, template: Option<&TemplateRecord>) {
        Self::append_with_optional_template(
            "job_invalidated",
            job,
            template,
            None,
            None,
            Some(now_record_timestamps().ts),
            "INVALIDATED",
        );
    }

    fn append(row: PoolJobRow<'_>) {
        let Some(writer) = global_writer() else {
            return;
        };
        let _ = writer.append_json_line(&row.to_value());
        let _ = writer.flush();
    }

    fn append_with_optional_template(
        event: &'static str,
        job: &PublishedJobSnapshot,
        template: Option<&TemplateRecord>,
        clean_jobs: Option<bool>,
        winner_preview_txid: Option<String>,
        retired_at: Option<String>,
        job_state: &str,
    ) {
        let ts = now_record_timestamps();
        let (record_id, f_set_size) = template
            .map(|t| (t.record_id.to_string(), t.gbt.transactions.len() as u64))
            .unwrap_or_else(|| (job.record_id.to_string(), 0));
        let row = PoolJobValue {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            event: event.to_string(),
            job_id: job.job_id.0.clone(),
            record_id,
            candidate_block_id: Some(job.candidate_block_id.0),
            node_height: job.height,
            tip_hash: job.tip_hash.0.clone(),
            previousblockhash: job.previousblockhash.clone(),
            job_state: job_state.to_string(),
            convergence_root_hex: Some(job.coinbase.convergence_root_hex.clone()),
            mint_required: Some(job.body_mint.mint_required),
            f_set_size: Some(f_set_size),
            assembly_hash: Some(hex::encode(job.assembly_hash)),
            template_height: job.height,
            clean_jobs,
            published_at: None,
            retired_at,
            winner_preview_txid,
        };
        if let Some(writer) = global_writer() {
            let _ = writer.append_json_line(&row);
            let _ = writer.flush();
        }
    }
}

struct PoolJobRow<'a> {
    event: &'static str,
    job: &'a PublishedJobSnapshot,
    template: &'a TemplateRecord,
    clean_jobs: Option<bool>,
    winner_preview_txid: Option<String>,
    mint_required: Option<bool>,
    retired_at: Option<String>,
    job_state: &'static str,
}

impl<'a> PoolJobRow<'a> {
    fn to_value(&self) -> PoolJobValue {
        let ts = now_record_timestamps();
        PoolJobValue {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts.clone(),
            event: self.event.to_string(),
            job_id: self.job.job_id.0.clone(),
            record_id: self.template.record_id.to_string(),
            candidate_block_id: Some(self.job.candidate_block_id.0),
            node_height: self.job.height,
            tip_hash: self.job.tip_hash.0.clone(),
            previousblockhash: self.job.previousblockhash.clone(),
            job_state: self.job_state.to_string(),
            convergence_root_hex: Some(self.job.coinbase.convergence_root_hex.clone()),
            mint_required: self.mint_required,
            f_set_size: Some(self.template.gbt.transactions.len() as u64),
            assembly_hash: Some(hex::encode(self.job.assembly_hash)),
            template_height: self.job.height,
            clean_jobs: self.clean_jobs,
            published_at: Some(ts.ts),
            retired_at: self.retired_at.clone(),
            winner_preview_txid: self.winner_preview_txid.clone(),
        }
    }
}

#[derive(Serialize)]
struct PoolJobValue {
    schema_version: &'static str,
    ts: String,
    event: String,
    job_id: String,
    record_id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    candidate_block_id: Option<u64>,
    node_height: u64,
    tip_hash: String,
    previousblockhash: String,
    job_state: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    convergence_root_hex: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    mint_required: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    f_set_size: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    assembly_hash: Option<String>,
    template_height: u64,
    #[serde(skip_serializing_if = "Option::is_none")]
    clean_jobs: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    published_at: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    retired_at: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    winner_preview_txid: Option<String>,
}

fn job_state_str(state: &JobState) -> &'static str {
    match state {
        JobState::Created => "CREATED",
        JobState::Active => "ACTIVE",
        JobState::Stale => "STALE",
        JobState::Invalidated => "INVALIDATED",
        JobState::Archived => "ARCHIVED",
    }
}
