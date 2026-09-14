use block_assembler::{JobPublisher, JobSnapshotApi};
use rigor_gbt::{GbtTemplate, TemplateCache, TemplateState};
use share_validator::{
    get_opus_hash_from_base_hash, rgrpow_hash80, ShareSubmission, ShareValidator,
    ValidationErrorCode, ValidationKind,
};
use std::sync::Arc;

fn fixture(longpoll: &str) -> (GbtTemplate, String) {
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "0100000000010000000001", "txid": "{a}", "hash": "{a}"}},
                {{"data": "0100000000020000000001", "txid": "{b}", "hash": "{b}"}}
            ],
            "coinbasevalue": 12800000000,
            "longpollid": "{longpoll}",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 200,
            "version": 536870912,
            "curtime": 1700000200,
            "default_witness_commitment": "6a24aa21a9ed"
        }}"#,
        prev = "00".repeat(32),
        a = "aa".repeat(32),
        b = "bb".repeat(32),
    );
    (serde_json::from_str(&raw).unwrap(), raw)
}

fn tip(b: u8) -> rigor_gbt::TipHash {
    let mut bytes = [0u8; 32];
    bytes[31] = b;
    rigor_gbt::TipHash(hex::encode(bytes))
}

#[test]
fn assembler_validator_convergence_consistency() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let (t, raw) = fixture("asm/val/1");
    let record = templates.ingest(t, raw, tip(1)).unwrap().unwrap();
    assert_eq!(record.template_state, TemplateState::Active);
    let job = publisher.publish_from_template(&record).unwrap();

    let expected_ss = block_assembler::script::build_coinbase_scriptsig(record.height);
    assert_eq!(hex::decode(&job.coinbase.scriptsig_hex).unwrap(), expected_ss);
    assert_eq!(hex::decode(&job.coinbase.convergence_root_hex).unwrap(), vec![0u8; 32]);
    assert!(job.body_mint.body_mint_txid.is_none());
    assert!(!job.body_mint.reserved);
    assert!(!job.body_mint.mint_required);

    let _api = JobSnapshotApi::new(Arc::new(block_assembler::JobCache::new()));
    let _ = validator.get_validation_state(&job.job_id);
}

#[test]
fn rejects_stale_job_after_reorg() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs.clone(), templates.clone());

    let (t, raw) = fixture("reorg/sv/1");
    let record = templates.ingest(t, raw, tip(0x0a)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    publisher.on_tip_changed(tip(0x0b)).unwrap();

    let share = ShareSubmission {
        share_id: "share-reorg-1".into(),
        job_id: job.job_id.clone(),
        record_id: Some(record.record_id),
        extranonce1: vec![0x01],
        extranonce2: vec![0x00, 0x00, 0x00, 0x00],
        ntime: job.ntime as u32,
        nonce: 0,
        share_target: [0xff; 32],
    };
    let result = validator.validate_share(share);
    assert!(!result.accepted);
    assert_eq!(result.error_code, Some(ValidationErrorCode::ErrStaleJob));
}

#[test]
fn rejects_replay_share_id() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let (t, raw) = fixture("replay/1");
    let record = templates.ingest(t, raw, tip(2)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    let share = ShareSubmission {
        share_id: "dup-share".into(),
        job_id: job.job_id.clone(),
        record_id: None,
        extranonce1: vec![],
        extranonce2: vec![0; job.coinbase.extranonce2_size as usize],
        ntime: job.ntime as u32,
        nonce: 1,
        share_target: [0xff; 32],
    };

    let r1 = validator.validate_share(share.clone());
    let r2 = validator.validate_share(share);
    if r1.accepted {
        assert!(!r2.accepted);
        assert_eq!(r2.error_code, Some(ValidationErrorCode::ErrReplayShare));
    } else {
        assert!(!r2.accepted);
    }
}

#[test]
fn opuhash_golden_vectors_are_historical_not_node_pow() {
    let mut base = [0u8; 32];
    base[0] = 1;
    let pow = get_opus_hash_from_base_hash(&base);
    let mut expect =
        hex::decode("0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780").unwrap();
    expect.reverse();
    assert_eq!(pow.as_slice(), expect.as_slice());
    let sha3 = rgrpow_hash80(&[0u8; 80]);
    assert_ne!(pow, sha3);
}

#[test]
fn rgrpow_zero_header_matches_rigord() {
    let want = hex::decode("dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc")
        .unwrap();
    assert_eq!(rgrpow_hash80(&[0u8; 80]).as_slice(), want.as_slice());
}

#[test]
fn accepts_share_when_pow_and_bindings_pass() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let (t, raw) = fixture("accept/1");
    let record = templates.ingest(t, raw, tip(4)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    let share = ShareSubmission {
        share_id: "accept-share-1".into(),
        job_id: job.job_id.clone(),
        record_id: Some(record.record_id),
        extranonce1: vec![],
        extranonce2: vec![0; job.coinbase.extranonce2_size as usize],
        ntime: job.ntime as u32,
        nonce: 0,
        share_target: [0xff; 32],
    };
    let result = validator.validate_share(share);
    assert!(
        result.accepted,
        "expected accept, got {:?}",
        result.message
    );
    assert!(result.pow_hash.is_some());
}

#[test]
fn rejects_share_when_ntime_does_not_match_job() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let (t, raw) = fixture("ntime/mismatch/1");
    let record = templates.ingest(t, raw, tip(5)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    let share = ShareSubmission {
        share_id: "ntime-mismatch-1".into(),
        job_id: job.job_id.clone(),
        record_id: Some(record.record_id),
        extranonce1: vec![],
        extranonce2: vec![0; job.coinbase.extranonce2_size as usize],
        ntime: job.ntime as u32 + 1,
        nonce: 0,
        share_target: [0xff; 32],
    };
    let result = validator.validate_share(share);
    assert!(!result.accepted);
    assert_eq!(
        result.error_code,
        Some(ValidationErrorCode::ErrMalformedSubmission)
    );
}

#[test]
fn accepts_share_with_historical_window_txid_and_opaque_mint() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let mint = "cc".repeat(32);
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{"data": "02000000000100", "txid": "{mint}", "hash": "{mint}", "weight": 681}}
            ],
            "coinbasevalue": 12800000000,
            "longpollid": "window/mint/1",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 306,
            "version": 536870912,
            "curtime": 1700000200,
            "default_witness_commitment": "6a24aa21a9ed"
        }}"#,
        prev = "00".repeat(32),
        mint = mint,
    );
    let t: GbtTemplate = serde_json::from_str(&raw).unwrap();
    let record = templates.ingest(t, raw, tip(6)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();
    assert!(job.body_mint.body_mint_txid.is_none());
    assert!(!job.body_mint.mint_required);

    let share = ShareSubmission {
        share_id: "window-mint-share-1".into(),
        job_id: job.job_id.clone(),
        record_id: Some(record.record_id),
        extranonce1: vec![],
        extranonce2: vec![0; job.coinbase.extranonce2_size as usize],
        ntime: job.ntime as u32,
        nonce: 0,
        share_target: [0xff; 32],
    };
    let result = validator.validate_share(share);
    assert!(
        result.accepted,
        "A-9-C historical window + node automint must accept, got {:?}",
        result.message
    );
}

#[test]
fn rejects_lottery_gbt_fields_at_ingest() {
    let templates = Arc::new(TemplateCache::new());
    let prev = "bb".repeat(32);
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [],
            "coinbasevalue": 12800000000,
            "longpollid": "lottery/1",
            "weightlimit": 1000000,
            "bits": "207fffff",
            "prev_nbits": "1c00ff00",
            "height": 306,
            "version": 536870912,
            "curtime": 1700000200,
            "default_witness_commitment": "6a24aa21a9ed",
            "convergence_draw": {{"source_blockhash": "{prev}", "source_height": 305, "sigma_seed": "{prev}", "payout_height": 306, "convergence_root": "{root}", "target_prev": "1c00ff00", "res_probability_target": "{root}", "eligible_root": "{root}"}}
        }}"#,
        prev = prev,
        root = "00".repeat(32),
    );
    let t: GbtTemplate = serde_json::from_str(&raw).unwrap();
    let err = templates.ingest(t, raw, tip(7)).unwrap_err();
    let msg = format!("{err}");
    assert!(msg.contains("V-06") || msg.contains("lottery"), "{msg}");
}

#[test]
fn leftover_convergence_root_check_is_noop() {
    let templates = Arc::new(TemplateCache::new());
    let (t, raw) = fixture("conv/mis/1");
    let record = templates.ingest(t, raw, tip(5)).unwrap().unwrap();
    share_validator::convergence::validate_convergence_root_matches(
        &record,
        "00".repeat(64).as_str(),
    )
    .unwrap();
}

#[test]
fn rejects_invalid_opuhash_on_impossible_target() {
    let templates = Arc::new(TemplateCache::new());
    let jobs = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(jobs.clone());
    let validator = ShareValidator::with_defaults(jobs, templates.clone());

    let (t, raw) = fixture("pow/1");
    let record = templates.ingest(t, raw, tip(3)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    let share = ShareSubmission {
        share_id: "low-pow".into(),
        job_id: job.job_id.clone(),
        record_id: None,
        extranonce1: vec![],
        extranonce2: vec![0; job.coinbase.extranonce2_size as usize],
        ntime: job.ntime as u32,
        nonce: 42,
        share_target: [0x00; 32],
    };
    let result = validator.validate_share(share);
    assert!(!result.accepted);
    assert!(
        result.error_code == Some(ValidationErrorCode::ErrInvalidOpouhash)
            || result.error_code == Some(ValidationErrorCode::ErrConvergenceMismatch)
            || result.error_code == Some(ValidationErrorCode::ErrMalformedSubmission)
    );
    assert_eq!(result.kind, ValidationKind::Share);
}
