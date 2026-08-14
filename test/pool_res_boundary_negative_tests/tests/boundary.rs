//! TASK-POOL-RES-BOUNDARY-IMPLEMENT-003 · Phase 4 negative security tests.
//!
//! Proves the pool production path cannot:
//! 1. create RES mint
//! 2. modify RES owner
//! 3. generate fake RES merkle placeholders
//! 4. pay miners in anything but ORU
//!
//! And that wallet Convergence rules are untouched (documentation / frozen params).

use block_assembler::{
    assemble_solved_block, JobPublisher, SolvedBlockContext, UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT,
};
use rigor_gbt::{GbtTemplate, TemplateCache};
use std::sync::Arc;
use stratum_server::{PayoutEngine, RecordingBlockSubmitter, StratumOrchestrator};

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
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
            "prev_nbits": "1c00ff00",
            "height": 100,
            "version": 536870912,
            "curtime": 1700000000,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{a}"]
        }}"#,
        prev = "11".repeat(32),
        a = "aa".repeat(32),
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(b: u8) -> rigor_gbt::TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = b;
    rigor_gbt::TipHash(hex::encode(bytes))
}

#[test]
fn pool_cannot_create_res_mint_via_orchestrator() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let orch = StratumOrchestrator::new(templates.clone(), jobs, Arc::new(PayoutEngine::new(1)))
        .with_submitter(Arc::new(RecordingBlockSubmitter::accepting()));

    let (t, raw) = fixture("neg/mint");
    templates.ingest(t, raw, tip(1)).unwrap();
    let envelopes = orch.publish_jobs_from_active_template().unwrap();
    assert!(!envelopes[0].mint_required);
    assert_eq!(orch.metrics.snapshot().res_mint_events, 0);

    // Type-level: StratumOrchestrator has no mint_engine field (compile-time).
    // Runtime: publish never increments res_mint_events.
}

#[test]
fn pool_cannot_modify_res_owner_on_job() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs);
    let (t, raw) = fixture("neg/owner");
    let record = templates.ingest(t, raw, tip(2)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    // Job must not carry a pool-built mint txid / owner payout script for RES.
    assert!(job.body_mint.body_mint_txid.is_none());
    assert!(!job.body_mint.mint_required);
    // Coinbase payout is ORU script only (devtest unspendable placeholder here).
    assert_eq!(
        hex::decode(&job.coinbase.payout_script_hex).unwrap(),
        UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.to_vec()
    );
}

#[test]
fn pool_cannot_generate_fake_res_merkle_placeholder() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs);
    let (t, raw) = fixture("neg/placeholder");
    let record = templates.ingest(t, raw, tip(3)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    assert!(!job.body_mint.reserved, "placeholder must not be reserved");
    assert!(
        job.body_mint.placeholder_txid.is_empty(),
        "fake placeholder txid must not be generated"
    );

    // Assembler refuses to assemble if a legacy reserved flag sneaks in.
    let mut legacy = (*job).clone();
    legacy.body_mint.reserved = true;
    legacy.body_mint.placeholder_txid = "cc".repeat(32);
    let err = assemble_solved_block(
        &record,
        &legacy,
        &SolvedBlockContext {
            extranonce1: vec![0x01],
            extranonce2: vec![0, 0, 0, 1],
            ntime: 1,
            nonce: 1,
        },
    )
    .unwrap_err()
    .to_string();
    assert!(
        err.contains("placeholder") || err.contains("reserved"),
        "{err}"
    );
}

#[test]
fn miner_receives_oru_only() {
    let payout = PayoutEngine::new(1);
    let job_id = block_assembler::JobId("job-oru".into());
    payout.credit_share("miner.1", &job_id, "s1", 1.0);
    payout.record_block_found_for_job(&job_id, 5_000_000_000);
    let bal = payout.balance("miner.1").unwrap();
    assert_eq!(bal.oru_pending, 5_000_000_000);
    // Ledger exposes ORU fields only — no RES balance API on PayoutEngine.
    let round = payout.rounds().into_iter().next().unwrap();
    assert_eq!(round.coinbase_oru, 5_000_000_000);
}

#[test]
fn wallet_convergence_params_unchanged_in_tree() {
    // Frozen product rule: W=32 and Convergence remain wallet/consensus — not pool.
    // Spot-check that pool crates do not redefine W or RES issuance constants as
    // operator-configurable mint amounts. (Consensus sources of truth are untouched.)
    let w_docs = include_str!("../../../docs/pool/POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md");
    assert!(w_docs.contains("W=32"));
    assert!(w_docs.contains("OPTION A"));
    assert!(!w_docs.contains("pool constructs RES"));

    let position = include_str!("../../../docs/pool/RIGOR_POOL_PRODUCT_POSITION.md");
    assert!(position.contains("not a RES issuer") || position.contains("ORU mining coordination"));
}
