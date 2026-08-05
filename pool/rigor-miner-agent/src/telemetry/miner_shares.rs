use std::sync::{Arc, OnceLock};

use rigor_pool_metrics::{
    now_record_timestamps, JsonlMetricsWriter, SCHEMA_VERSION,
};
use serde::Serialize;

use crate::metrics::MetricsSnapshot;

const MINER_SHARES_FILE: &str = "miner_shares.jsonl";

fn global_writer() -> Option<Arc<JsonlMetricsWriter>> {
    static WRITER: OnceLock<Option<Arc<JsonlMetricsWriter>>> = OnceLock::new();
    WRITER
        .get_or_init(|| {
            JsonlMetricsWriter::open_default(MINER_SHARES_FILE)
                .ok()
                .map(Arc::new)
        })
        .clone()
}

#[derive(Clone)]
pub struct MinerTelemetry {
    writer: Option<Arc<JsonlMetricsWriter>>,
}

impl Default for MinerTelemetry {
    fn default() -> Self {
        Self {
            writer: global_writer(),
        }
    }
}

impl MinerTelemetry {
    pub fn disabled() -> Self {
        Self { writer: None }
    }

    pub fn record_snapshot(
        &self,
        worker_id: &str,
        hashrate: f64,
        snap: &MetricsSnapshot,
    ) {
        let Some(writer) = &self.writer else {
            return;
        };
        let ts = now_record_timestamps();
        let row = MinerSnapshotRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            ts_ms: ts.ts_ms,
            source: "MINER_AGENT",
            worker_id: worker_id.to_string(),
            event: "metrics_snapshot",
            hashrate,
            hashes_total: snap.hashes,
            shares_found: snap.shares_found,
            shares_submitted: snap.shares_submitted,
            shares_accepted: snap.shares_accepted,
            shares_rejected: snap.shares_rejected,
            stale_jobs: snap.stale_jobs,
            uptime_seconds: snap.uptime_secs,
        };
        let _ = writer.append_json_line(&row);
        let _ = writer.flush();
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShareResultEvent {
    Accepted,
    Rejected,
    Stale,
}

impl ShareResultEvent {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Accepted => "accepted",
            Self::Rejected => "rejected",
            Self::Stale => "stale",
        }
    }
}

pub struct MinerShareTelemetry;

impl MinerShareTelemetry {
    pub fn record_share(
        worker_id: &str,
        job_id: &str,
        node_height: u64,
        result: ShareResultEvent,
        reject_reason: Option<&str>,
        difficulty: Option<f64>,
    ) {
        let Some(writer) = global_writer() else {
            return;
        };
        let ts = now_record_timestamps();
        let row = ShareEventRow {
            schema_version: SCHEMA_VERSION,
            ts: ts.ts,
            ts_ms: ts.ts_ms,
            source: "MINER_AGENT",
            worker_id: worker_id.to_string(),
            job_id: job_id.to_string(),
            node_height,
            share_result: result.as_str().to_string(),
            reject_reason: reject_reason.map(|s| s.to_string()),
            difficulty,
        };
        let _ = writer.append_json_line(&row);
    }
}

#[derive(Serialize)]
struct MinerSnapshotRow {
    schema_version: &'static str,
    ts: String,
    ts_ms: u64,
    source: &'static str,
    worker_id: String,
    event: &'static str,
    hashrate: f64,
    hashes_total: u64,
    shares_found: u64,
    shares_submitted: u64,
    shares_accepted: u64,
    shares_rejected: u64,
    stale_jobs: u64,
    uptime_seconds: u64,
}

#[derive(Serialize)]
struct ShareEventRow {
    schema_version: &'static str,
    ts: String,
    ts_ms: u64,
    source: &'static str,
    worker_id: String,
    job_id: String,
    node_height: u64,
    share_result: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    reject_reason: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    difficulty: Option<f64>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn snapshot_row_serializes() {
        let snap = MetricsSnapshot {
            hashrate: 1.0,
            hashes: 10,
            shares_found: 0,
            shares_submitted: 0,
            shares_accepted: 0,
            shares_rejected: 0,
            stale_jobs: 0,
            uptime_secs: 5,
        };
        let t = MinerTelemetry::disabled();
        t.record_snapshot("w", 1.0, &snap);
    }
}
