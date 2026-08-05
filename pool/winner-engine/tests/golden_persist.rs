//! P-03 golden vector persistence round-trip (TASK-MINING-017 §5).

use rigor_gbt::RecordId;
use winner_engine::{
    draw_input_from_golden, get_winner_by_candidate, persist_golden_draw, replay_convergence_draw,
    CandidateBlockRecord, CandidateBlockStatus, WinnerPgBackend,
};

fn load_vectors() -> serde_json::Value {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../formal_verification/phase3_golden_vectors.json"
    );
    let raw = std::fs::read_to_string(path).expect("phase3_golden_vectors.json");
    serde_json::from_str(&raw).expect("parse golden json")
}

#[test]
fn golden_run_selection_persist_memory_roundtrip() {
    let data = load_vectors();
    let mut backend = WinnerPgBackend::memory();

    for v in data["vectors"].as_array().unwrap() {
        if v["module"] != "RunSelection" {
            continue;
        }
        let inp = &v["input"];
        let txids: Vec<String> = inp["txids_hex"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_str().unwrap().to_string())
            .collect();
        let fees: Vec<u64> = inp["fees"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_u64().unwrap())
            .collect();
        let height = inp["height"].as_u64().unwrap();
        let prev_hex = inp["prev_block_hash_hex"].as_str().unwrap();
        let t_hex = inp["T_hex"].as_str().unwrap();
        let input = draw_input_from_golden(&txids, &fees, height as u32, prev_hex, t_hex).unwrap();
        let draw = replay_convergence_draw(&input);
        let candidate = CandidateBlockRecord {
            id: 0,
            job_id: format!("golden-{}", v["id"].as_str().unwrap()),
            record_id: RecordId::new_v4(),
            prev_block_hash: "00".repeat(64),
            tip_hash: "ff".repeat(64),
            height,
            convergence_root_display: draw.convergence_root_display.clone(),
            mint_required: draw.mint_required,
            status: CandidateBlockStatus::Active,
            body_mint_reserved: true,
        };
        let (cand_id, winner_id) = persist_golden_draw(&mut backend, candidate, &draw).unwrap();
        assert!(cand_id > 0);
        assert!(winner_id > 0);
        let job = get_winner_by_candidate(&backend, cand_id).unwrap().unwrap();
        assert_eq!(
            job.preview_audit.convergence_root_display,
            draw.convergence_root_display
        );
        assert_eq!(job.mint_required, draw.mint_required);
        assert_eq!(job.eligible_count, draw.eligible_count);
    }
}

#[cfg(feature = "postgres")]
#[test]
#[ignore = "requires DATABASE_URL and live PostgreSQL"]
fn golden_run_selection_persist_postgres_roundtrip() {
    let _ = std::env::var("DATABASE_URL").expect("DATABASE_URL for postgres golden test");
    let mut backend = WinnerPgBackend::open().expect("open backend");
    assert!(backend.is_postgres());

    let data = load_vectors();
    let v = data["vectors"]
        .as_array()
        .unwrap()
        .iter()
        .find(|v| v["module"] == "RunSelection")
        .expect("RunSelection vector");
    let inp = &v["input"];
    let txids: Vec<String> = inp["txids_hex"]
        .as_array()
        .unwrap()
        .iter()
        .map(|x| x.as_str().unwrap().to_string())
        .collect();
    let fees: Vec<u64> = inp["fees"]
        .as_array()
        .unwrap()
        .iter()
        .map(|x| x.as_u64().unwrap())
        .collect();
    let input = draw_input_from_golden(
        &txids,
        &fees,
        inp["height"].as_u64().unwrap() as u32,
        inp["prev_block_hash_hex"].as_str().unwrap(),
        inp["T_hex"].as_str().unwrap(),
    )
    .unwrap();
    let draw = replay_convergence_draw(&input);
    let candidate = CandidateBlockRecord {
        id: 0,
        job_id: "pg-golden-001".into(),
        record_id: RecordId::new_v4(),
        prev_block_hash: "00".repeat(64),
        tip_hash: "ff".repeat(64),
        height: inp["height"].as_u64().unwrap(),
        convergence_root_display: draw.convergence_root_display.clone(),
        mint_required: draw.mint_required,
        status: CandidateBlockStatus::Active,
        body_mint_reserved: true,
    };
    let (cand_id, _) = persist_golden_draw(&mut backend, candidate, &draw).unwrap();
    let job = get_winner_by_candidate(&backend, cand_id).unwrap().unwrap();
    assert_eq!(
        job.preview_audit.convergence_root_display,
        draw.convergence_root_display
    );
}
