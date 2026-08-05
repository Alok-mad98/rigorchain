use rigor_pool_pg::{PoolBackend, RecoveryEngine, EQUIVALENCE_CLAIMED};

#[test]
fn memory_backend_open_and_metrics() {
    let backend = PoolBackend::open().expect("open");
    assert!(!backend.is_postgres());
    let m = backend.metrics_snapshot();
    assert_eq!(m.pg_inserts, 0);
}

#[test]
fn recovery_memory_noop() {
    let mut backend = PoolBackend::memory();
    let report = RecoveryEngine::check_tip_and_recover(&mut backend, "aa".repeat(64).as_str())
        .expect("recover");
    assert!(!report.tip_mismatch);
    assert!(!report.reorg_applied);
}

#[test]
fn equivalence_claimed_false() {
    assert!(!EQUIVALENCE_CLAIMED);
}
