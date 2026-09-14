use rigor_gbt::{GbtTemplate, TipHash};
use rigor_pool_pilot::{PilotTick, PoolPilot};

/// Owner program the fixture convergence transactions declare.
const OWNER_PROGRAM: [u8; 32] = [0xa1; 32];

fn res_owner_declaration_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut script = Vec::with_capacity(1 + 1 + 4 + 1 + 32);
    script.push(0x6a);
    script.push(37);
    script.extend_from_slice(b"RESO");
    script.push(1);
    script.extend_from_slice(program32);
    script
}

fn convergence_tx_declaring(program32: &[u8; 32]) -> String {
    let mut cvg2_script = vec![0x51u8, 0x20, 0x43, 0x56, 0x47, 0x32];
    cvg2_script.extend_from_slice(&[0x00; 28]);
    let declaration = res_owner_declaration_script(program32);

    let mut raw = Vec::new();
    raw.extend_from_slice(&2i32.to_le_bytes());
    raw.push(0x01);
    raw.extend_from_slice(&[0x11; 32]);
    raw.extend_from_slice(&0u32.to_le_bytes());
    raw.push(0x00);
    raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
    raw.push(0x02);
    for (value, script) in [(100_000_000i64, cvg2_script), (0i64, declaration)] {
        raw.extend_from_slice(&value.to_le_bytes());
        raw.push(script.len() as u8);
        raw.extend_from_slice(&script);
    }
    raw.extend_from_slice(&0u32.to_le_bytes());
    hex::encode(raw)
}

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
    let tx_hex = convergence_tx_declaring(&OWNER_PROGRAM);
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "{tx_hex}", "txid": "{a}", "hash": "{a}"}},
                {{"data": "{tx_hex}", "txid": "{b}", "hash": "{b}"}}
            ],
            "coinbasevalue": 12800000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 300,
            "version": 536870912,
            "curtime": 1700000300,
            "default_witness_commitment": "6a24aa21a9ed"
        }}"#,
        prev = "00".repeat(32),
        a = "aa".repeat(32),
        b = "bb".repeat(32),
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(b: u8) -> TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = b;
    TipHash(hex::encode(bytes))
}

#[test]
fn full_pipeline_job_publish_and_stratum_subscribe() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t, raw) = fixture("pilot/e2e/1");
    let tick = pilot.ingest_fixture(t, raw, tip(1)).unwrap();
    assert!(matches!(tick, PilotTick::TemplatePublished { .. }));

    let envelopes = pilot.orchestrator.publish_jobs_from_active_template().unwrap();
    assert_eq!(envelopes.len(), 1);
    assert!(!envelopes[0].mint_required);

    let sub = r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-rgrpow01",null]}"#;
    let out = pilot.orchestrator.handle_request(1, sub).unwrap();
    assert!(out[0].contains("\"result\""));

    let auth = r#"{"id":2,"method":"mining.authorize","params":["miner.worker","x"]}"#;
    let out2 = pilot.orchestrator.handle_request(1, auth).unwrap();
    assert!(out2[0].contains("true"));

    let report = pilot.status_report().unwrap();
    assert!(report.active_jobs >= 1);
}

#[test]
fn pool_has_no_res_mint_engine_on_reorg() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t, raw) = fixture("pilot/e2e/mint");
    let tick = pilot.ingest_fixture(t, raw, tip(0x0a)).unwrap();
    assert!(matches!(tick, PilotTick::TemplatePublished { .. }));

    let job = pilot
        .jobs
        .get_active_jobs()
        .unwrap()
        .into_iter()
        .next()
        .expect("active job");
    assert!(!job.body_mint.reserved);
    assert!(!job.body_mint.mint_required);

    pilot.simulate_tip_change(tip(0x0b)).unwrap();
    let report = pilot.status_report().unwrap();
    assert!(report.audit_events.iter().any(|e| e.category == "reorg"));
    assert_eq!(report.metrics.res_mint_events, 0);
}

/// Regression: after adapter tip-change + ingest, only invalidate jobs — do not wipe
/// the freshly ingested ACTIVE template (that caused job2 broadcast then immediate
/// Invalidated / "no active template" / mass stale rejects).
#[test]
fn post_adapter_tip_invalidation_keeps_active_for_publish() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t1, raw1) = fixture("pilot/e2e/tip-a");
    pilot.ingest_fixture(t1, raw1, tip(0x0a)).unwrap();
    assert_eq!(pilot.jobs.get_active_jobs().unwrap().len(), 1);

    // Mirror GbtAdapter::poll_once tip path: clear then ingest replacement ACTIVE.
    pilot.templates.handle_tip_changed(tip(0x0b)).unwrap();
    let (t2, raw2) = fixture("pilot/e2e/tip-b");
    let ingested = pilot
        .templates
        .ingest(t2, raw2, tip(0x0b))
        .unwrap()
        .expect("replacement template");
    assert_eq!(ingested.template_state, rigor_gbt::TemplateState::Active);
    assert!(pilot.templates.get_active_template().unwrap().is_some());

    pilot
        .orchestrator
        .invalidate_jobs_on_tip_change(tip(0x0b))
        .unwrap();
    assert!(
        pilot.templates.get_active_template().unwrap().is_some(),
        "ACTIVE must survive job-only tip invalidation"
    );
    let envelopes = pilot
        .orchestrator
        .publish_jobs_from_active_template()
        .expect("publish must succeed with surviving ACTIVE");
    assert!(!envelopes.is_empty());
}

#[test]
fn share_submit_pipeline() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t, raw) = fixture("pilot/e2e/share");
    pilot.ingest_fixture(t, raw, tip(2)).unwrap();
    let envelopes = pilot.orchestrator.publish_jobs_from_active_template().unwrap();
    let job_id = &envelopes[0].job_id;

    pilot
        .orchestrator
        .handle_request(
            1,
            r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-rgrpow01",null]}"#,
        )
        .unwrap();
    pilot
        .orchestrator
        .handle_request(
            1,
            r#"{"id":2,"method":"mining.authorize","params":["bob.worker","x"]}"#,
        )
        .unwrap();

    let submit = format!(
        r#"{{"id":3,"method":"mining.submit","params":["bob.worker","{job_id}","00000000","00000000","00000001"]}}"#
    );
    let out = pilot.orchestrator.handle_request(1, &submit);
    let metrics = pilot.orchestrator.metrics.snapshot();
    assert!(metrics.shares_accepted + metrics.shares_rejected >= 1);
    if metrics.blocks_found > 0 {
        assert!(out.is_err(), "unsubmitted block did not surface an error");
        assert_eq!(metrics.blocks_submitted, 0);
        assert_eq!(pilot.orchestrator.payout.total_liability_oru(), 0);
    } else {
        assert!(!out.unwrap().is_empty());
    }
}

#[test]
fn pplns_oru_only_on_block_found() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t, raw) = fixture("pilot/e2e/pplns");
    pilot.ingest_fixture(t, raw, tip(3)).unwrap();
    let envelopes = pilot.orchestrator.publish_jobs_from_active_template().unwrap();
    let job_id = block_assembler::JobId(envelopes[0].job_id.clone());
    pilot
        .orchestrator
        .payout
        .credit_share("w", &job_id, "s1", 1.0);
    pilot
        .orchestrator
        .payout
        .record_block_found_for_job(&job_id, 5_000_000_000);
    let bal = pilot.orchestrator.payout.balance("w").unwrap();
    assert_eq!(bal.oru_pending, 5_000_000_000);

    pilot
        .orchestrator
        .payout
        .record_block_found_for_job(&block_assembler::JobId("next".into()), 5_000_000_000);
    assert_eq!(
        pilot.orchestrator.payout.balance("w").unwrap().oru_pending,
        5_000_000_000
    );
}
