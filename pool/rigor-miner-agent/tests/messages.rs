use rigor_miner_agent::{MiningNotify, MiningNotifyRigor, StratumEvent};
use serde_json::json;

#[test]
fn mining_notify_parses_nine_params() {
    let params = json!([
        "job-1",
        "00".repeat(64),
        "0100000001",
        "0200000002",
        ["aabb", "ccdd"],
        "20000000",
        "207fffff",
        "65000000",
        true
    ]);
    let notify = MiningNotify::parse(&params).unwrap();
    assert_eq!(notify.job_id, "job-1");
    assert_eq!(notify.merkle_branch, vec!["aabb", "ccdd"]);
    assert!(notify.clean_jobs);
}

#[test]
fn mining_notify_rigor_parses_convergence_metadata() {
    let params = json!({
        "job_id": "job-1",
        "convergence_root_hex": "00".repeat(64),
        "preview_winner_txid": "aa".repeat(64),
        "mint_required": true
    });
    let rigor = MiningNotifyRigor::parse(&params).unwrap();
    assert_eq!(rigor.job_id, "job-1");
    assert!(rigor.convergence.mint_required);
    assert!(rigor.convergence.preview_winner_txid.is_some());
}

#[test]
fn stratum_event_from_notify_line() {
    let line = format!(
        r#"{{"id":null,"method":"mining.notify","params":["j1","{}","c1","c2",[],"20000000","207fffff","65000000",false]}}"#,
        "00".repeat(64)
    );
    let msg = rigor_miner_agent::parse_line(&line).unwrap();
    let ev = match msg.method.as_deref() {
        Some("mining.notify") => StratumEvent::Notify(
            MiningNotify::parse(&msg.params.unwrap()).unwrap(),
        ),
        _ => panic!("expected notify"),
    };
    if let StratumEvent::Notify(n) = ev {
        assert_eq!(n.job_id, "j1");
    }
}
