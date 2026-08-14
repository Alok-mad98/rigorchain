//! TASK-MINING-021 memory drift / job rotation soak (in-memory pilot).

use rigor_gbt::{GbtTemplate, TipHash};
use rigor_pool_pilot::{PilotTick, PoolPilot};

fn fixture(longpoll: &str, height: u64) -> (GbtTemplate, String) {
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "0100000000010000000001", "txid": "{a}", "hash": "{a}", "fee": 100000000}},
                {{"data": "0100000000020000000001", "txid": "{b}", "hash": "{b}", "fee": 100000000}}
            ],
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": {height},
            "version": 536870912,
            "curtime": 1700000300,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{a}", "{b}"]
        }}"#,
        prev = "00".repeat(32),
        a = "aa".repeat(32),
        b = "bb".repeat(32),
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(byte: u8) -> TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = byte;
    TipHash(hex::encode(bytes))
}

#[test]
fn job_rotation_memory_drift_1000() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let mut last_job_id = String::new();
    let mut publish_count = 0usize;

    for i in 0..1000 {
        let (t, raw) = fixture(&format!("stability-021/{i}"), 300 + i);
        let tick = pilot
            .ingest_fixture(t, raw, tip((i % 200) as u8))
            .expect("ingest");
        if let PilotTick::TemplatePublished { job_id } = tick {
            publish_count += 1;
            last_job_id = job_id;
            let envelopes = pilot
                .orchestrator
                .publish_jobs_from_active_template()
                .expect("publish");
            assert!(!envelopes.is_empty());
            let root_a = envelopes[0].convergence_root_hex.clone();
            let root_b = pilot
                .orchestrator
                .jobs
                .envelope_for_job_id(&envelopes[0].job_id)
                .expect("envelope")
                .convergence_root_hex;
            assert_eq!(root_a, root_b, "root drift at rotation {i}");
        }
    }

    assert!(publish_count >= 900, "expected frequent publishes, got {publish_count}");
    assert!(!last_job_id.is_empty());
    let report = pilot.status_report().expect("status");
    assert!(report.active_jobs >= 1);
}
