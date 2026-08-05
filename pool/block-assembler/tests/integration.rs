use block_assembler::{JobPublisher, JobSnapshotApi, JobState};
use rigor_gbt::{GbtTemplate, TemplateCache, TemplateState, TemplateVersion, TipHash};
use std::sync::Arc;

fn fixture(longpoll: &str, conv_a: &str, conv_b: &str) -> (GbtTemplate, String) {
    let raw = format!(
        r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{
                    "data": "0100000000010000000001",
                    "txid": "{conv_a}",
                    "hash": "{conv_a}"
                }},
                {{
                    "data": "0100000000020000000001",
                    "txid": "{conv_b}",
                    "hash": "{conv_b}"
                }}
            ],
            "coinbasevalue": 5000000000,
            "longpollid": "{longpoll}",
            "weightlimit": 4000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 150,
            "version": 536870912,
            "curtime": 1700000100,
            "default_witness_commitment": "6a24aa21a9ed",
            "default_convergence_commitment": "6a2d",
            "convergence_txids": ["{conv_a}", "{conv_b}"]
        }}"#,
        prev = "00".repeat(32),
    );
    let template: GbtTemplate = serde_json::from_str(&raw).unwrap();
    (template, raw)
}

fn tip(label: u8) -> TipHash {
    let mut b = [0u8; 32];
    b[31] = label;
    TipHash(hex::encode(b))
}

#[test]
fn end_to_end_template_cache_to_job_publish() {
    let template_cache = Arc::new(TemplateCache::new());
    let job_cache = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(job_cache.clone());
    let api = JobSnapshotApi::new(job_cache);

    let (t, raw) = fixture("int/1", &"aa".repeat(32), &"bb".repeat(32));
    let record = template_cache.ingest(t, raw, tip(0x01)).unwrap().unwrap();
    assert_eq!(record.template_state, TemplateState::Active);

    let job = publisher.publish_from_template(&record).unwrap();
    assert_eq!(job.state, JobState::Active);
    assert_eq!(job.record_id, record.record_id);
    assert_eq!(job.candidate_block_id.0, job.job_id.0.parse::<u64>().unwrap());

    let active = api.get_active_jobs().unwrap();
    assert_eq!(active.len(), 1);
    assert_eq!(active[0].job_id, job.job_id);

    let history = api.get_job_history().unwrap();
    assert_eq!(history.len(), 1);
}

#[test]
fn reorg_invalidates_jobs_and_allows_new_publish() {
    let template_cache = Arc::new(TemplateCache::new());
    let job_cache = Arc::new(block_assembler::JobCache::new());
    let publisher = JobPublisher::with_defaults(job_cache.clone());

    let (t1, raw1) = fixture("reorg/1", &"aa".repeat(32), &"bb".repeat(32));
    let r1 = template_cache.ingest(t1, raw1, tip(0x0a)).unwrap().unwrap();
    let j1 = publisher.publish_from_template(&r1).unwrap();

    publisher.on_tip_changed(tip(0x0b)).unwrap();
    template_cache.handle_tip_changed(tip(0x0b)).unwrap();

    let stale_job = job_cache.get_job_by_id(&j1.job_id).unwrap().unwrap();
    assert_eq!(stale_job.state, JobState::Invalidated);

    let (t2, raw2) = fixture("reorg/2", &"cc".repeat(32), &"dd".repeat(32));
    let r2 = template_cache.ingest(t2, raw2, tip(0x0b)).unwrap().unwrap();
    let j2 = publisher.publish_from_template(&r2).unwrap();
    assert_eq!(j2.state, JobState::Active);
    assert_ne!(j1.job_id, j2.job_id);
    assert_eq!(j2.template_version, TemplateVersion::INITIAL);
}

#[test]
fn body_mint_placeholder_is_immutable_and_reserved() {
    let template_cache = Arc::new(TemplateCache::new());
    let publisher = JobPublisher::with_defaults(Arc::new(block_assembler::JobCache::new()));

    let (t, raw) = fixture("mint/1", &"aa".repeat(32), &"bb".repeat(32));
    let record = template_cache.ingest(t, raw, tip(1)).unwrap().unwrap();
    let job = publisher.publish_from_template(&record).unwrap();

    assert!(job.body_mint.reserved);
    assert!(!job.body_mint.mint_required);
    assert!(job.body_mint.body_mint_txid.is_none());
    assert_eq!(job.body_mint.slot_index, 3);
    let placeholder = job.body_mint.placeholder_txid.clone();
    let reread = publisher.cache().get_job_by_id(&job.job_id).unwrap().unwrap();
    assert_eq!(reread.body_mint.placeholder_txid, placeholder);

    let (t2, raw2) = fixture("mint/2", &"cc".repeat(32), &"dd".repeat(32));
    let record2 = template_cache.ingest(t2, raw2, tip(1)).unwrap().unwrap();
    let job2 = publisher.publish_from_template(&record2).unwrap();
    assert_ne!(job2.job_id, job.job_id);
    assert_ne!(job2.body_mint.placeholder_txid, placeholder);
}
