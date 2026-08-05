use mint_engine::{MintBlockContext, MintState, MintWinnerContext};
use rigor_gbt::{GbtTemplate, TipHash};
use rigor_pool_pilot::{PilotTick, PoolPilot};

/// Owner program the fixture convergence transactions declare.
const OWNER_PROGRAM: [u8; 32] = [0xa1; 32];

/// SECURITY-001-A / A-5: a convergence tx enters the draw only if it declares its RES payout
/// owner, and the mint must pay that program, so fixtures carry real declaring transactions.
fn convergence_tx_declaring(program32: &[u8; 32]) -> String {
    let mut cvg2_script = vec![0x51u8, 0x20, 0x43, 0x56, 0x47, 0x32]; // OP_1 <32> "CVG2"
    cvg2_script.extend_from_slice(&[0x00; 28]);
    let declaration = mint_engine::res_owner::res_owner_declaration_script(program32);

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
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 4000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 300,
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

    // Devtest profile uses the shared-password authenticator; no pre-registration.
    let sub = r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#;
    let out = pilot
        .orchestrator
        .handle_request(1, sub)
        .unwrap();
    assert!(out[0].contains("\"result\""));

    let auth = r#"{"id":2,"method":"mining.authorize","params":["miner.worker","x"]}"#;
    let out2 = pilot.orchestrator.handle_request(1, auth).unwrap();
    assert!(out2[0].contains("true"));

    let report = pilot.status_report().unwrap();
    assert!(report.active_jobs >= 1);
}

#[test]
fn res_mint_and_reorg_revocation() {
    let mut pilot = PoolPilot::in_memory_simulation().unwrap();
    let (t, raw) = fixture("pilot/e2e/mint");
    let tick = pilot.ingest_fixture(t, raw, tip(0x0a)).unwrap();
    let job_id = match tick {
        PilotTick::TemplatePublished { job_id } => job_id,
        other => panic!("expected publish, got {other:?}"),
    };

    let record = pilot
        .templates
        .get_active_template()
        .unwrap()
        .unwrap();
    let job = pilot
        .jobs
        .get_job_by_id(&block_assembler::JobId(job_id.clone()))
        .unwrap()
        .unwrap();

    let preview = MintWinnerContext {
        mint_required: true,
        winner_txid: Some("aa".repeat(32)),
        convergence_root_hex: job.coinbase.convergence_root_hex.clone(),
        draw_target_prev_nbits: "1c00ff00".into(),
        authorized_bind_hash: Some([0x11; 32]),
        authorized_bind_hash_display: Some("11".repeat(32)),
    };

    let outcome = pilot
        .orchestrator
        .mint_engine
        .mint_for_winner(
            &preview,
            record.as_ref(),
            job.as_ref(),
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 1,
                nonce: 1,
            },
        )
        .unwrap()
        .unwrap();
    // Six proof pushes plus the P2WSH redeem script.
    assert_eq!(
        outcome.artifact.witness_item_count,
        mint_engine::PCORB_WITNESS_ITEMS + 1
    );
    assert!(!outcome.artifact.sigma_digest_hex.is_empty());
    assert_eq!(outcome.block.vtx_count, 4);

    pilot.simulate_tip_change(tip(0x0b)).unwrap();
    let rec = pilot
        .orchestrator
        .mint_engine
        .get_record(&job_id)
        .unwrap()
        .unwrap();
    assert_eq!(rec.state, MintState::Revoked);

    let report = pilot.status_report().unwrap();
    assert!(report
        .audit_events
        .iter()
        .any(|e| e.category == "reorg" || e.category == "res.mint.revoked"));
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
            r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
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
    // C-5: with the devtest (refusing) submission sink, a found block must surface
    // as an error and must never book a payout round.
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
    // C-6e: bind the round to the real job id so REORG-01 rollback can match it.
    pilot
        .orchestrator
        .payout
        .record_block_found_for_job(&job_id, 5_000_000_000);
    let bal = pilot.orchestrator.payout.balance("w").unwrap();
    assert_eq!(bal.oru_pending, 5_000_000_000);

    // C-6a: the window is consumed, so a second block pays this share nothing more.
    pilot
        .orchestrator
        .payout
        .record_block_found_for_job(&block_assembler::JobId("next".into()), 5_000_000_000);
    assert_eq!(
        pilot.orchestrator.payout.balance("w").unwrap().oru_pending,
        5_000_000_000
    );
}
