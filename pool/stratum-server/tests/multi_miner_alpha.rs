//! Multi-miner ORU PPLNS simulation for Alpha product readiness.
//! Consensus impact: NONE. RES involvement: NONE.

use block_assembler::JobId;
use stratum_server::PayoutEngine;

#[test]
fn multi_miner_pplns_weights_three_workers() {
    let engine = PayoutEngine::with_fee_percent(1, 1.0); // 1% pool fee
    let job = JobId("multi-1".into());
    engine.credit_share("alice.rig1", &job, "s1", 10.0);
    engine.credit_share("bob.rig1", &job, "s2", 20.0);
    engine.credit_share("carol.rig1", &job, "s3", 30.0);
    engine.record_block_found_for_job(&job, 1_000_000);

    // 1% fee → 990_000 miner pot; weights 10:20:30 → 165k : 330k : 495k
    assert_eq!(engine.pool_fee_retained_oru(), 10_000);
    assert_eq!(engine.balance("alice.rig1").unwrap().oru_pending, 165_000);
    assert_eq!(engine.balance("bob.rig1").unwrap().oru_pending, 330_000);
    assert_eq!(engine.balance("carol.rig1").unwrap().oru_pending, 495_000);
    assert_eq!(engine.total_liability_oru(), 990_000);
}

#[test]
fn multi_miner_restart_safe_ledger_hash_stable_when_idle() {
    let engine = PayoutEngine::new(7);
    let job = JobId("r1".into());
    engine.credit_share("a", &job, "1", 1.0);
    engine.credit_share("b", &job, "2", 1.0);
    engine.record_block_found_for_job(&job, 100);
    let h1 = engine.ledger_state_hash();
    let h2 = engine.ledger_state_hash();
    assert_eq!(h1, h2);
    assert_eq!(engine.balance("a").unwrap().oru_pending, 50);
    assert_eq!(engine.balance("b").unwrap().oru_pending, 50);
}
