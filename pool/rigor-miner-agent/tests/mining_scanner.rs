use std::time::Instant;

use rigor_miner_agent::{
    build_header_bytes, CpuNonceScanner, Metrics, MiningNotify, ScanConfig,
};
use share_validator::verify_opuhash_share;

fn regtest_notify() -> MiningNotify {
    MiningNotify {
        job_id: "scan-test".into(),
        prevhash: "00".repeat(32),
        coinb1: "01".into(),
        coinb2: "02".into(),
        merkle_branch: vec![],
        version: "20000000".into(),
        nbits: "207fffff".into(),
        ntime: "65000000".into(),
        clean_jobs: false,
    }
}

#[test]
fn scanner_extranononce_differs_per_thread() {
    let a = ScanConfig {
        thread_id: 0,
        thread_count: 3,
        extranonce1: vec![0x01],
        extranonce2_size: 4,
        batch_size: 3,
    };
    let b = ScanConfig {
        thread_id: 2,
        thread_count: 3,
        ..a.clone()
    };
    assert_ne!(a.extranonce2_for_thread(), b.extranonce2_for_thread());
}

#[test]
fn scanner_finds_pow_under_max_target() {
    let notify = regtest_notify();
    let target = [0xff; 32];
    let metrics = Metrics::new();
    let mut found = None;

    for nonce in 0..50_000u32 {
        let header =
            build_header_bytes(&notify, &[0x01], &[0, 0, 0, 1], 0x6500_0000, nonce).unwrap();
        let base = rigor_miner_agent::header_base_hash(&header);
        let (_, ok) = verify_opuhash_share(&base, &target);
        metrics.record_hash();
        if ok {
            found = Some(nonce);
            metrics.record_share_found();
            break;
        }
    }

    assert!(
        found.is_some(),
        "expected to find a share within 50k nonces on max target"
    );
}

#[test]
fn scanner_batch_records_hashes() {
    use rigor_miner_agent::{JobSnapshot, ConvergenceMetadata};

    let job = JobSnapshot {
        notify: regtest_notify(),
        convergence: ConvergenceMetadata::default(),
        difficulty: 1e-9,
        received_at: Instant::now(),
        stale: false,
    };
    let metrics = Metrics::new();
    let mut scanner = CpuNonceScanner::new(ScanConfig {
        thread_id: 0,
        thread_count: 1,
        extranonce1: vec![0x01],
        extranonce2_size: 4,
        batch_size: 100,
    });
    let _ = scanner.scan_batch(&job, &metrics);
    assert!(metrics.hashes() >= 100);
}
