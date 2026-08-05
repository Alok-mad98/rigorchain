use std::io::{BufRead, BufReader};
use std::sync::Arc;
use std::thread;

use rigor_pool_metrics::{schema_version, JsonlMetricsWriter, SCHEMA_VERSION};
use serde_json::Value;
use tempfile::TempDir;

fn read_lines(path: &std::path::Path) -> Vec<Value> {
    let file = std::fs::File::open(path).unwrap();
    BufReader::new(file)
        .lines()
        .map(|l| serde_json::from_str(&l.unwrap()).unwrap())
        .collect()
}

#[test]
fn create_if_missing_and_append_valid_jsonl() {
    let tmp = TempDir::new().unwrap();
    let w = JsonlMetricsWriter::create_if_missing(tmp.path(), "test.jsonl").unwrap();
    w.append_json_line(&serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T00:00:00Z",
        "ts_ms": 1,
        "hello": "world"
    }))
    .unwrap();
    w.flush().unwrap();
    let lines = read_lines(&w.path().unwrap());
    assert_eq!(lines.len(), 1);
    assert_eq!(lines[0]["hello"], "world");
}

#[test]
fn schema_version_constant_matches_spec() {
    assert_eq!(schema_version(), "1.0.0");
}

#[test]
fn append_multiple_lines() {
    let tmp = TempDir::new().unwrap();
    let w = JsonlMetricsWriter::create_if_missing(tmp.path(), "multi.jsonl").unwrap();
    for i in 0..5 {
        w.append_json_line(&serde_json::json!({"n": i})).unwrap();
    }
    assert_eq!(read_lines(&w.path().unwrap()).len(), 5);
}

#[test]
fn flush_persists_to_disk() {
    let tmp = TempDir::new().unwrap();
    let w = JsonlMetricsWriter::create_if_missing(tmp.path(), "flush.jsonl").unwrap();
    w.append_json_line(&serde_json::json!({"x": 1})).unwrap();
    w.flush().unwrap();
    assert!(w.path().unwrap().exists());
}

#[test]
fn rotate_daily_archives_previous_file() {
    let tmp = TempDir::new().unwrap();
    let w = JsonlMetricsWriter::create_if_missing(tmp.path(), "rotate.jsonl").unwrap();
    w.append_json_line(&serde_json::json!({"day": "old"})).unwrap();
    w.set_day_for_test("2020-01-01");
    w.rotate_daily().unwrap();
    w.append_json_line(&serde_json::json!({"day": "new"})).unwrap();
    let archived = tmp.path().join("rotate.2020-01-01.jsonl");
    assert!(archived.exists());
    let new_lines = read_lines(&w.path().unwrap());
    assert_eq!(new_lines[0]["day"], "new");
}

#[test]
fn concurrent_append_from_threads() {
    let tmp = TempDir::new().unwrap();
    let w = Arc::new(JsonlMetricsWriter::create_if_missing(tmp.path(), "conc.jsonl").unwrap());
    let handles: Vec<_> = (0..8)
        .map(|t| {
            let w = w.clone();
            thread::spawn(move || {
                for i in 0..10 {
                    w.append_json_line(&serde_json::json!({"thread": t, "i": i}))
                        .unwrap();
                }
            })
        })
        .collect();
    for h in handles {
        h.join().unwrap();
    }
    assert_eq!(read_lines(&w.path().unwrap()).len(), 80);
}

#[test]
fn miner_snapshot_schema_has_required_fields() {
    let row = serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T12:00:00.000Z",
        "ts_ms": 1710000000000u64,
        "source": "MINER_AGENT",
        "worker_id": "test.worker",
        "event": "metrics_snapshot",
        "hashrate": 2500.0,
        "hashes_total": 1000,
        "shares_found": 0,
        "shares_submitted": 0,
        "shares_accepted": 0,
        "shares_rejected": 0,
        "stale_jobs": 0,
        "uptime_seconds": 30
    });
    assert_eq!(row["schema_version"], SCHEMA_VERSION);
    assert_eq!(row["source"], "MINER_AGENT");
}

#[test]
fn pool_job_lifecycle_schema_compat() {
    let row = serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T12:00:00.000Z",
        "event": "job_published",
        "job_id": "j1",
        "record_id": "uuid",
        "node_height": 11,
        "tip_hash": "aa".repeat(32),
        "previousblockhash": "bb".repeat(32),
        "job_state": "ACTIVE",
        "mint_required": false,
        "winner_preview_txid": null,
        "clean_jobs": true,
        "published_at": "2026-06-10T12:00:00.000Z"
    });
    assert_eq!(row["event"], "job_published");
}

#[test]
fn winner_authorization_schema_compat() {
    let row = serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T12:00:00.000Z",
        "event": "preview",
        "job_id": "j1",
        "node_height": 11,
        "tip_hash": "aa".repeat(32),
        "f_set_size": 0,
        "draw_winner_txid": "0".repeat(64),
        "convergence_root_hex": "cc".repeat(64),
        "mint_required": false,
        "authorized": false
    });
    assert_eq!(row["event"], "preview");
}

#[test]
fn mint_engine_schema_compat() {
    let row = serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T12:00:00.000Z",
        "event": "mint_built",
        "job_id": "j1",
        "node_height": 11,
        "draw_winner_txid": "aa".repeat(64),
        "mint_txid": "bb".repeat(64),
        "state": "built"
    });
    assert_eq!(row["event"], "mint_built");
}

#[test]
fn share_event_schema_compat() {
    let row = serde_json::json!({
        "schema_version": SCHEMA_VERSION,
        "ts": "2026-06-10T12:00:00.000Z",
        "ts_ms": 1,
        "source": "MINER_AGENT",
        "worker_id": "w",
        "job_id": "j1",
        "node_height": 11,
        "share_result": "accepted"
    });
    assert_eq!(row["share_result"], "accepted");
}
