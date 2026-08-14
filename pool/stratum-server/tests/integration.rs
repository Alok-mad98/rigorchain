use block_assembler::JobPublisher;
use rigor_gbt::{GbtTemplate, TemplateCache};
use std::sync::Arc;
use stratum_server::{
    HashedCredentialAuthenticator, PayoutEngine, RecordingBlockSubmitter, SessionManager,
    StratumOrchestrator, VardiffConfig,
};

/// Test credential material. H-5a: the pool no longer accepts a shared `"x"`
/// password, so tests must register a worker against a real credential store.
const TEST_SALT: &[u8] = b"integration-test-salt-0";
const TEST_PW: &str = "integration-test-secret";

fn authenticator(workers: &[&str]) -> Arc<SessionManager> {
    let auth = HashedCredentialAuthenticator::new();
    for w in workers {
        auth.register(w, TEST_PW, TEST_SALT).unwrap();
    }
    Arc::new(SessionManager::with_authenticator(Arc::new(auth)))
}

/// C-5 mining fixtures only try 64 nonces. Production vardiff starts at 1024, which
/// will not produce a block candidate in that window. Use the devtest controller
/// (`start_difficulty = 1e-5`) so the test exercises submit/credit, not PoW grind.
fn authenticator_devtest(workers: &[&str]) -> Arc<SessionManager> {
    let auth = HashedCredentialAuthenticator::new();
    for w in workers {
        auth.register(w, TEST_PW, TEST_SALT).unwrap();
    }
    // Easier than documented devtest 1e-5: that still requires ~2^32*D hashes
    // (tens of thousands of OpuHash). C-5 only needs the submit/credit path.
    let mut vd = VardiffConfig::devtest();
    vd.start_difficulty = 1e-18;
    vd.min_difficulty = 1e-18;
    Arc::new(SessionManager::with_authenticator_and_vardiff(Arc::new(auth), vd))
}

/// Owner program the fixture convergence transactions declare.
const OWNER_PROGRAM: [u8; 32] = [0xa1; 32];

/// Inline RESO declaration (mirrors consensus layout; no mint-engine dependency).
fn res_owner_declaration_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut script = Vec::with_capacity(1 + 1 + 4 + 1 + 32);
    script.push(0x6a); // OP_RETURN
    script.push(37); // payload size
    script.extend_from_slice(b"RESO");
    script.push(1); // version
    script.extend_from_slice(program32);
    script
}

/// SECURITY-001-A / A-5: a convergence tx enters the draw only if it declares its RES payout
/// owner. Fixtures carry declaring transactions for GBT realism; the pool does not mint RES.
fn convergence_tx_declaring(program32: &[u8; 32]) -> String {
    let mut cvg2_script = vec![0x51u8, 0x20, 0x43, 0x56, 0x47, 0x32]; // OP_1 <32> "CVG2"
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


/// Parent-draw JSON whose winner/eligible flags match pool A-8 replay (not hardcoded index 0).
fn parent_draw_json(prev: &str, height: u64, candidates: &[(&str, u64)]) -> String {
    use winner_engine::{
        display_hex_to_native, native_to_display_hex, replay_convergence_draw, DrawReplayInput,
        RES_PROBABILITY_TARGET,
    };
    let candidate_txids: Vec<[u8; 32]> = candidates
        .iter()
        .map(|(txid, _)| display_hex_to_native(txid).unwrap())
        .collect();
    let weights: Vec<u64> = candidates.iter().map(|(_, w)| *w).collect();
    let replay = replay_convergence_draw(&DrawReplayInput {
        candidate_txids,
        weights: weights.clone(),
        prev_block_hash: display_hex_to_native(prev).unwrap(),
        payout_height: height as u32,
        res_probability_target: RES_PROBABILITY_TARGET,
    });
    let cand_json: Vec<String> = candidates
        .iter()
        .enumerate()
        .map(|(i, (txid, w))| {
            format!(
                r#"{{"txid":"{txid}","weight":{w},"res_program":"{prog}","eligible":{elig}}}"#,
                txid = txid,
                w = w,
                prog = "11".repeat(32),
                elig = if replay.candidates[i].draw_eligible {
                    "true"
                } else {
                    "false"
                },
            )
        })
        .collect();
    let winner = if replay.winner_index >= 0 {
        let i = replay.winner_index as usize;
        let (txid, w) = candidates[i];
        format!(
            r#"{{"index":{i},"txid":"{txid}","weight":{w},"res_program":"{prog}"}}"#,
            i = i,
            txid = txid,
            w = w,
            prog = "11".repeat(32),
        )
    } else {
        "null".into()
    };
    format!(
        r#"{{
            "source_blockhash": "{prev}",
            "source_height": {source_height},
            "sigma_seed": "{prev}",
            "payout_height": {height},
            "convergence_root": "{root}",
            "target_prev": "1c00ff00",
            "res_probability_target": "{t}",
            "eligible_root": "{elig}",
            "candidates": [{cands}],
            "winner": {winner}
        }}"#,
        prev = prev,
        source_height = height.saturating_sub(1),
        height = height,
        root = native_to_display_hex(&replay.convergence_root),
        t = hex::encode(RES_PROBABILITY_TARGET),
        elig = "dd".repeat(32),
        cands = cand_json.join(","),
        winner = winner,
    )
}

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
    let tx_hex = convergence_tx_declaring(&OWNER_PROGRAM);
    let prev = "ab".repeat(32);
    let a = "aa".repeat(32);
    let b = "bb".repeat(32);
    let draw = parent_draw_json(
        &prev,
        300,
        &[(&a, 100_000_000 * 65536), (&b, 100_000_000 * 65536)],
    );
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "{tx_hex}", "txid": "{a}", "hash": "{a}"}},
                {{"data": "{tx_hex}", "txid": "{b}", "hash": "{b}"}}
            ],
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 300,
            "version": 536870912,
            "curtime": 1700000300,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{a}", "{b}"],
            "convergence_draw": {draw}
        }}"#,
        prev = prev,
        a = a,
        b = b,
        tx_hex = tx_hex,
        longpoll = longpoll,
        draw = draw,
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(b: u8) -> rigor_gbt::TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = b;
    rigor_gbt::TipHash(hex::encode(bytes))
}

fn stack() -> StratumOrchestrator {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    StratumOrchestrator::new(templates, jobs, Arc::new(PayoutEngine::new(1)))
        .with_sessions(authenticator(&["alice.worker1", "bob.worker"]))
}

#[test]
fn subscribe_authorize_flow() {
    let orch = stack();
    let sub = r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#;
    let out = orch.handle_request(1, sub).unwrap();
    assert!(!out.is_empty());
    assert!(out[0].contains("\"result\""));

    let auth = format!(
        r#"{{"id":2,"method":"mining.authorize","params":["alice.worker1","{TEST_PW}"]}}"#
    );
    let out2 = orch.handle_request(1, &auth).unwrap();
    assert!(out2[0].contains("true"));
    // H-5d: the miner is told its share difficulty on authorize.
    assert!(
        out2.iter().any(|l| l.contains("mining.set_difficulty")),
        "expected set_difficulty, got {out2:?}"
    );
}

/// H-5a regression: the historical hardcoded `"x"` password must be refused.
#[test]
fn hardcoded_x_password_is_refused() {
    let orch = stack();
    orch.handle_request(
        1,
        r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
    )
    .unwrap();
    let out = orch
        .handle_request(1, r#"{"id":2,"method":"mining.authorize","params":["alice.worker1","x"]}"#)
        .unwrap();
    assert!(out[0].contains("error"), "got {out:?}");
    assert!(orch.metrics.snapshot().auth_failures >= 1);
}

/// H-5b regression: a share must be credited to the authenticated session
/// identity, never to the worker named in the request parameters.
#[test]
fn share_cannot_be_attributed_to_another_worker() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let payout = Arc::new(PayoutEngine::new(1));
    let orch = StratumOrchestrator::new(templates.clone(), jobs, payout.clone())
        .with_sessions(authenticator(&["alice.worker1", "victim.worker"]));

    let (t, raw) = fixture("impersonate/1");
    templates.ingest(t, raw, tip(1)).unwrap();
    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    let job_id = &envelopes[0].job_id;

    orch.handle_request(
        1,
        r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
    )
    .unwrap();
    orch.handle_request(
        1,
        &format!(r#"{{"id":2,"method":"mining.authorize","params":["alice.worker1","{TEST_PW}"]}}"#),
    )
    .unwrap();

    // alice claims to be victim.worker in the submit parameters.
    let submit = format!(
        r#"{{"id":3,"method":"mining.submit","params":["victim.worker","{job_id}","00000000","00000000","00000001"]}}"#
    );
    let out = orch.handle_request(1, &submit).unwrap();
    assert!(out[0].contains("error"), "got {out:?}");
    assert!(orch.metrics.snapshot().impersonation_attempts >= 1);
    // No balance was created for either identity by this submission.
    assert!(payout.balance("victim.worker").is_none());
}

/// H-5b regression: an unauthorized session cannot submit at all.
#[test]
fn unauthorized_session_cannot_submit() {
    let orch = stack();
    orch.handle_request(
        1,
        r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
    )
    .unwrap();
    let out = orch
        .handle_request(
            1,
            r#"{"id":3,"method":"mining.submit","params":["alice.worker1","j","00000000","00000000","00000001"]}"#,
        )
        .unwrap();
    assert!(out[0].contains("error"), "got {out:?}");
}

#[test]
fn rejects_non_opushash_agent() {
    let orch = stack();
    let sub = r#"{"id":1,"method":"mining.subscribe","params":["bad-miner",null]}"#;
    let out = orch.handle_request(1, sub).unwrap();
    assert!(out[0].contains("error"));
}

#[test]
fn full_pipeline_publish_notify_submit() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let payout = Arc::new(PayoutEngine::new(1));
    // C-5: the default orchestrator now fails closed with no submission endpoint,
    // so the happy-path pipeline test must supply one.
    let orch = StratumOrchestrator::new(templates.clone(), jobs.clone(), payout)
        .with_sessions(authenticator(&["bob.worker"]))
        .with_submitter(Arc::new(RecordingBlockSubmitter::accepting()));

    let (t, raw) = fixture("strat/1");
    templates.ingest(t, raw, tip(1)).unwrap();

    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    assert_eq!(envelopes.len(), 1);
    assert_eq!(envelopes[0].notify.method, "mining.notify");
    // Option A: product surface never advertises mint_required / RES payout.
    assert!(!envelopes[0].mint_required);
    assert!(envelopes[0].preview_winner_txid.is_none());

    orch.handle_request(
        1,
        r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
    )
    .unwrap();
    orch.handle_request(
        1,
        &format!(r#"{{"id":2,"method":"mining.authorize","params":["bob.worker","{TEST_PW}"]}}"#),
    )
    .unwrap();

    let job_id = &envelopes[0].job_id;
    let ntime = envelopes[0]
        .notify
        .params
        .as_array()
        .and_then(|a| a.get(6))
        .and_then(|v| v.as_str())
        .unwrap_or("00000000");
    let submit = format!(
        r#"{{"id":3,"method":"mining.submit","params":["bob.worker","{job_id}","00000000","{ntime}","00000001"]}}"#
    );
    let out = orch.handle_request(1, &submit).unwrap();
    assert!(!out.is_empty());
    let metrics = orch.metrics.snapshot();
    assert!(metrics.shares_accepted + metrics.shares_rejected >= 1);
    // C-5 invariant: any block found on this path reached the node.
    assert_eq!(metrics.unaccounted_blocks(), 0, "{metrics:?}");
}

#[test]
fn reorg_orphans_payout_shares() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let payout = Arc::new(PayoutEngine::new(1));
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let orch = StratumOrchestrator::new(templates.clone(), jobs, payout.clone());

    let (t, raw) = fixture("reorg/s");
    let record = templates.ingest(t, raw, tip(0x0a)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();
    payout.credit_share("w", &job.job_id, "s1", 1.0);

    orch.handle_tip_change(tip(0x0b)).unwrap();
    assert!(payout.balance("w").unwrap().shares_orphaned >= 1);
}

#[test]
fn reorg_does_not_invoke_pool_res_mint() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let payout = Arc::new(PayoutEngine::new(1));
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let orch = StratumOrchestrator::new(templates.clone(), jobs, payout.clone());

    let (t, raw) = fixture("mint/reorg");
    let record = templates.ingest(t, raw, tip(0x0a)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();
    payout.credit_share("w", &job.job_id, "s1", 1.0);

    orch.handle_tip_change(tip(0x0b)).unwrap();
    assert!(payout.balance("w").unwrap().shares_orphaned >= 1);
    // Orchestrator no longer exposes mint_engine; metrics stay at zero mint events.
    assert_eq!(orch.metrics.snapshot().res_mint_events, 0);
}

#[test]
fn job_expiry_invalidates_downstream() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());

    let (t1, raw1) = fixture("exp/1");
    let r1 = templates.ingest(t1, raw1, tip(1)).unwrap().unwrap();
    let j1 = publisher.publish_from_template(&r1).unwrap();

    let (t2, raw2) = fixture("exp/2");
    let r2 = templates.ingest(t2, raw2, tip(1)).unwrap().unwrap();
    let j2 = publisher.publish_from_template(&r2).unwrap();

    let old = jobs.get_job_by_id(&j1.job_id).unwrap().unwrap();
    assert_eq!(old.state, block_assembler::JobState::Stale);
    assert_eq!(jobs.get_active_jobs().unwrap().len(), 1);
    assert_eq!(jobs.get_active_jobs().unwrap()[0].job_id, j2.job_id);
}

/// C-5 regression: a found block must be offered to the node, and miner credit
/// must only appear after the node accepted it.
///
/// The regtest-easy fixture (`bits = 207fffff`) makes the first submitted share a
/// block candidate, which drives the whole found-block path.
#[test]
fn c5_block_is_submitted_and_credit_follows_node_acceptance() {
    for (accept, expect_credit) in [(true, true), (false, false)] {
        let templates = Arc::new(TemplateCache::new());
        let jobs = Arc::new(block_assembler::JobCache::new());
        let payout = Arc::new(PayoutEngine::new(1));
        let submitter = Arc::new(if accept {
            RecordingBlockSubmitter::accepting()
        } else {
            RecordingBlockSubmitter::rejecting()
        });
        let orch = StratumOrchestrator::new(templates.clone(), jobs, payout.clone())
            .with_sessions(authenticator_devtest(&["bob.worker"]))
            .with_submitter(submitter.clone());

        let (t, raw) = devtest_fixture("c5/submit");
        templates.ingest(t, raw, tip(0xc5)).unwrap();
        let envelopes = orch.publish_jobs_from_active_template().unwrap();
        let job_id = envelopes[0].job_id.clone();
        let ntime = envelopes[0]
            .notify
            .params
            .as_array()
            .and_then(|a| a.get(7))
            .and_then(|v| v.as_str())
            .unwrap_or("00000000")
            .to_string();

        orch.handle_request(
            1,
            r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
        )
        .unwrap();
        orch.handle_request(
            1,
            &format!(r#"{{"id":2,"method":"mining.authorize","params":["bob.worker","{TEST_PW}"]}}"#),
        )
        .unwrap();

        let mut found = false;
        for nonce in 0u32..64 {
            let submit = format!(
                r#"{{"id":3,"method":"mining.submit","params":["bob.worker","{job_id}","00000000","{ntime}","{nonce:08x}"]}}"#
            );
            let _ = orch.handle_request(1, &submit);
            if orch.metrics.snapshot().blocks_found > 0 {
                found = true;
                break;
            }
        }
        assert!(found, "fixture did not produce a block candidate: {:?}", orch.metrics.snapshot());

        let metrics = orch.metrics.snapshot();
        // The block reached the submission sink regardless of the RES mint outcome.
        assert!(
            submitter.submission_count() >= 1 || metrics.blocks_unsubmittable >= 1,
            "block was neither submitted nor reported unsubmittable: {metrics:?}"
        );

        if submitter.submission_count() >= 1 {
            let credited = payout.rounds().iter().any(|r| r.job_id.0 == job_id);
            assert_eq!(
                credited, expect_credit,
                "credit={credited} but node accept={accept} (metrics {metrics:?})"
            );
            if accept {
                assert_eq!(metrics.blocks_submitted, 1);
                assert_eq!(metrics.unaccounted_blocks(), 0);
            } else {
                assert_eq!(metrics.blocks_submitted, 0);
                assert!(metrics.blocks_submit_failed >= 1);
                assert!(metrics.unaccounted_blocks() >= 1);
            }
        }
    }
}

/// C-5 regression: with no submission endpoint configured the pool must refuse to
/// credit anybody. This is the exact scenario the audit found — the pool booked a
/// debt for blocks that never reached the network.
#[test]
fn c5_unconfigured_submitter_never_credits() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let payout = Arc::new(PayoutEngine::new(1));
    // Default orchestrator ships UnconfiguredBlockSubmitter.
    let orch = StratumOrchestrator::new(templates.clone(), jobs, payout.clone())
        .with_sessions(authenticator_devtest(&["bob.worker"]));

    let (t, raw) = devtest_fixture("c5/unconfigured");
    templates.ingest(t, raw, tip(0xc6)).unwrap();
    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    let job_id = envelopes[0].job_id.clone();

    orch.handle_request(
        1,
        r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-opushash",null]}"#,
    )
    .unwrap();
    orch.handle_request(
        1,
        &format!(r#"{{"id":2,"method":"mining.authorize","params":["bob.worker","{TEST_PW}"]}}"#),
    )
    .unwrap();

    let ntime = envelopes[0]
        .notify
        .params
        .as_array()
        .and_then(|a| a.get(7))
        .and_then(|v| v.as_str())
        .unwrap_or("00000000")
        .to_string();
    for nonce in 0u32..64 {
        let submit = format!(
            r#"{{"id":3,"method":"mining.submit","params":["bob.worker","{job_id}","00000000","{ntime}","{nonce:08x}"]}}"#
        );
        let _ = orch.handle_request(1, &submit);
        if orch.metrics.snapshot().blocks_found > 0 {
            break;
        }
    }

    let metrics = orch.metrics.snapshot();
    assert!(metrics.blocks_found > 0, "no block candidate produced: {metrics:?}");
    assert_eq!(metrics.blocks_submitted, 0);
    assert_eq!(payout.total_liability_oru(), 0, "credited an unsubmitted block");
    assert!(payout.rounds().is_empty(), "finalized a round without submission");
}

fn devtest_fixture(longpoll: &str) -> (GbtTemplate, String) {
    let prev = "ab".repeat(32);
    let a = "aa".repeat(32);
    let draw = parent_draw_json(&prev, 11, &[(&a, 100_000_000 * 65536)]);
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "0100000000010000000001", "txid": "{a}", "hash": "{a}"}}
            ],
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "207fffff",
            "height": 11,
            "version": 536870912,
            "curtime": 1700000300,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{a}"],
            "convergence_draw": {draw}
        }}"#,
        prev = prev,
        a = a,
        longpoll = longpoll,
        draw = draw,
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

#[test]
fn devtest_oru_only_publish_surface() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let orch = StratumOrchestrator::new(templates.clone(), jobs, Arc::new(PayoutEngine::new(1)));

    let (t, raw) = devtest_fixture("devtest/oru-only");
    templates.ingest(t, raw, tip(0xab)).unwrap();

    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    assert_eq!(envelopes.len(), 1);
    assert!(!envelopes[0].mint_required);
    assert!(envelopes[0].preview_winner_txid.is_none());
    assert_eq!(orch.metrics.snapshot().res_mint_events, 0);
}
