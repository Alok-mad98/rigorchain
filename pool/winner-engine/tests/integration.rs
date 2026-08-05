use rigor_gbt::{
    build_template_record, ConvergenceDraw, ConvergenceDrawCandidate, GbtRecord, GbtTransaction,
    RecordId, TemplateState, TemplateVersion, TipHash,
};
use winner_engine::{
    authorize_mint, extract_parent_draw_candidates, invalidate_winner, replay_convergence_draw,
    replay_draw_from_candidate_set, AuthorizationContext, CandidateBlockRecord,
    CandidateBlockStatus, CandidateRepository, DrawReplayInput, InMemoryWinnerStore,
    RevocationReason, WinnerJobStatus, WinnerRepository, RES_PROBABILITY_TARGET,
};

fn sample_parent_draw(candidates: Vec<(&str, u64)>) -> ConvergenceDraw {
    let list: Vec<ConvergenceDrawCandidate> = candidates
        .iter()
        .map(|(txid, weight)| ConvergenceDrawCandidate {
            txid: (*txid).into(),
            weight: *weight,
            res_program: "11".repeat(32),
            sigma: None,
            eligible: true,
        })
        .collect();
    let winner = list.first().map(|c| rigor_gbt::ConvergenceDrawWinner {
        index: 0,
        txid: c.txid.clone(),
        weight: c.weight,
        res_program: c.res_program.clone(),
    });
    ConvergenceDraw {
        source_blockhash: "ab".repeat(32),
        source_height: 119,
        sigma_seed: "ab".repeat(32),
        payout_height: 120,
        convergence_root: "cc".repeat(32),
        target_prev: "1c00ff00".into(),
        res_probability_target: hex::encode(RES_PROBABILITY_TARGET),
        eligible_root: "dd".repeat(32),
        candidates: list,
        winner,
    }
}

fn sample_template(convergence: Vec<&str>) -> rigor_gbt::TemplateRecord {
    let txs: Vec<GbtTransaction> = convergence
        .iter()
        .map(|txid| GbtTransaction {
            data: "01".into(),
            txid: (*txid).into(),
            hash: Some((*txid).into()),
            depends: None,
            fee: Some(100_000_000),
            sigops: None,
            weight: None,
        })
        .collect();
    let draw_candidates: Vec<(&str, u64)> = convergence.iter().map(|t| (*t, 100_000_000u64)).collect();
    let gbt = GbtRecord {
        record_id: RecordId::new_v4(),
        fetched_at: 0,
        tip_hash: TipHash("ff".repeat(32)),
        previousblockhash: "ab".repeat(32),
        height: 120,
        longpollid: "lp".into(),
        bits: "207fffff".into(),
        prev_nbits: "1c00ff00".into(),
        target: None,
        mintime: None,
        curtime: None,
        coinbasevalue: 50,
        transactions: txs,
        // Committed set for the *next* draw — distinct from the paid parent draw.
        convergence_txids: convergence.into_iter().map(|s| s.to_string()).collect(),
        convergence_root: None,
        convergence_draw: Some(sample_parent_draw(draw_candidates)),
        default_witness_commitment: None,
        default_convergence_commitment: None,
        weightlimit: Some(4_000_000),
        version: None,
        gbt_payload_hash: [0; 32],
        raw_json: "{}".into(),
    };
    build_template_record(gbt, None, TemplateVersion::INITIAL, TemplateState::Active)
}

#[test]
fn extract_parent_draw_preserves_node_order() {
    let a = "aa".repeat(32);
    let b = "bb".repeat(32);
    let tpl = sample_template(vec![&a, &b]);
    let set = extract_parent_draw_candidates(&tpl).unwrap();
    assert_eq!(set.txids_display, vec![a, b]);
    assert_eq!(set.weights.len(), 2);
    assert_eq!(set.height, 120);
    assert_eq!(set.source_height, 119);
}

#[test]
fn singleton_draw_mint_required_rfc015() {
    let tx = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    let input = DrawReplayInput {
        candidate_txids: vec![winner_engine::hash_hex_to_native(tx).unwrap()],
        weights: vec![1],
        prev_block_hash: winner_engine::hash_hex_to_native(
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        )
        .unwrap(),
        payout_height: 1,
        res_probability_target: winner_engine::hash_hex_to_native(
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        )
        .unwrap(),
    };
    let draw = replay_convergence_draw(&input);
    assert!(draw.mint_required);
    assert_eq!(draw.winner_index, 0);
    assert_eq!(draw.convergence_root, [0u8; 32]);
}

#[test]
fn authorization_happy_path() {
    let tx = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    let tpl = sample_template(vec![tx]);
    let set = extract_parent_draw_candidates(&tpl).unwrap();
    // Use a saturating T so eligibility is deterministic for this authorization fixture.
    let t_max = [0xffu8; 32];
    let draw = replay_draw_from_candidate_set(&set, t_max);

    let candidate = CandidateBlockRecord {
        id: 1,
        job_id: "0001".into(),
        record_id: tpl.record_id,
        prev_block_hash: tpl.gbt.previousblockhash.clone(),
        tip_hash: tpl.tip_hash.0.clone(),
        height: tpl.height,
        convergence_root_display: draw.convergence_root_display.clone(),
        mint_required: draw.mint_required,
        status: CandidateBlockStatus::Active,
        body_mint_reserved: true,
    };

    let ctx = AuthorizationContext {
        draw: draw.clone(),
        node_draw_convergence_root: set.convergence_root,
        candidate_set: set,
        candidate_block: candidate,
        template_bits: tpl.gbt.bits.clone(),
        template_prev_nbits: tpl.gbt.prev_nbits.clone(),
        current_tip: tpl.tip_hash.0.clone(),
        paused_new_jobs: false,
        treasury_available: true,
        now_unix: 1_000,
        consumed_token_nonces: vec![],
    };

    // AUTH-03 compares the replayed root against the node's reported parent root. For this
    // fixture they intentionally differ (replay recomputes C; the fixture supplies a fixed C),
    // so force agreement the way a real GBT path does after verify_replay_against_node.
    let mut ctx = ctx;
    ctx.node_draw_convergence_root = ctx.draw.convergence_root;
    ctx.candidate_set.convergence_root = ctx.draw.convergence_root;

    let token = authorize_mint(&ctx).unwrap();
    assert_eq!(token.status, WinnerJobStatus::Authorized);
    assert_ne!(token.bind_hash, [0u8; 32]);
}

#[test]
fn reorg_invalidates_winner_job() {
    let mut store = InMemoryWinnerStore::new();
    let block = CandidateBlockRecord {
        id: 0,
        job_id: "j1".into(),
        record_id: RecordId::new_v4(),
        prev_block_hash: "00".repeat(32),
        tip_hash: "ff".repeat(32),
        height: 10,
        convergence_root_display: "00".repeat(64),
        mint_required: true,
        status: CandidateBlockStatus::Active,
        body_mint_reserved: true,
    };
    let cid = CandidateRepository::insert_candidate(&mut store, block).unwrap();
    let draw = winner_engine::DrawReplayResult {
        convergence_root: [0; 32],
        convergence_root_display: "00".repeat(64),
        draw_winner_txid: Some([1; 32]),
        draw_winner_txid_display: Some("aa".repeat(64)),
        winner_index: 0,
        eligible_count: 1,
        mint_required: true,
        candidates: vec![],
        validation_method: winner_engine::ValidationMethod::Replay,
    };
    WinnerRepository::insert_winner_job(
        &mut store,
        winner_engine::WinnerJobRecord {
            id: 0,
            candidate_block_id: cid,
            status: WinnerJobStatus::Provisional,
            mint_required: draw.mint_required,
            eligible_count: 1,
            convergence_root_display: draw.convergence_root_display.clone(),
            winner_txid_display: draw.draw_winner_txid_display.clone(),
            preview_audit: draw,
            token_nonce: None,
        },
    )
    .unwrap();
    let mut job = store.get_by_candidate_block(cid).unwrap().unwrap();
    invalidate_winner(&mut job, RevocationReason::Reorg);
    WinnerRepository::update_status(&mut store, job.id, job.status).unwrap();
    let job = store.get_by_candidate_block(cid).unwrap().unwrap();
    assert_eq!(job.status, WinnerJobStatus::Stale);
}
