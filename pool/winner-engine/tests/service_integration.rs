//! WinnerRuntime integration for TASK-MINING-018.

use block_assembler::{
    BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    PublishedJobSnapshot,
};
use rigor_gbt::{
    ConvergenceDraw, ConvergenceDrawCandidate, ConvergenceDrawWinner, GbtRecord, GbtTransaction,
    RecordId, TemplateVersion, TipHash,
};
use winner_engine::{
    display_hex_to_native, native_to_display_hex, replay_convergence_draw, DrawReplayInput,
    RevocationReason, WinnerRuntime, RES_PROBABILITY_TARGET,
};

fn fixture_job(tpl: &rigor_gbt::TemplateRecord) -> PublishedJobSnapshot {
    let root = native_to_display_hex(&block_assembler::compute_convergence_root(
        &tpl.gbt.convergence_txids,
    ));
    PublishedJobSnapshot {
        job_id: JobId("job-018".into()),
        candidate_block_id: CandidateBlockId(42),
        record_id: RecordId::new_v4(),
        tip_hash: TipHash("aa".repeat(32)),
        template_version: TemplateVersion::INITIAL,
        previousblockhash: "ab".repeat(32),
        height: 300,
        state: JobState::Active,
        coinbase: CoinbaseInfo {
            coinb1_hex: "01".into(),
            coinb2_hex: "02".into(),
            extranonce2_size: 4,
            scriptsig_hex: "03".into(),
            convergence_root_hex: root,
            payout_script_hex: "76".into(),
            coinbase_value: 50,
        },
        commitments: CommitmentInfo {
            cvge_script_hex: None,
            rigor_script_hex: "6a08".into(),
            witness_commitment_script_hex: None,
            prev_nbits_hex: "1c00ff00".into(),
        },
        body_mint: BodyMintPlaceholder {
            slot_index: 2,
            reserved: true,
            mint_required: true,
            body_mint_txid: None,
            placeholder_txid: "cc".repeat(32),
        },
        merkle_branch: vec![],
        version: 1,
        nbits: "207fffff".into(),
        ntime: 0,
        prevhash: "ab".repeat(32),
        block_target: "207fffff".into(),
        assembly_hash: [0u8; 32],
        created_at: 0,
        state_changed_at: 0,
    }
}

/// Build a parent-draw fixture whose winner/eligible flags match pool replay (A-8).
fn parent_draw(txids: &[&str], height: u64) -> ConvergenceDraw {
    let weights: Vec<u64> = txids.iter().map(|_| 100_000_000 * 65536).collect();
    let prev = "ab".repeat(32);
    let candidate_txids: Vec<[u8; 32]> = txids
        .iter()
        .map(|t| display_hex_to_native(&t.repeat(32)).unwrap())
        .collect();
    let input = DrawReplayInput {
        candidate_txids,
        weights: weights.clone(),
        prev_block_hash: display_hex_to_native(&prev).unwrap(),
        payout_height: height as u32,
        res_probability_target: RES_PROBABILITY_TARGET,
    };
    let replay = replay_convergence_draw(&input);
    let candidates: Vec<ConvergenceDrawCandidate> = txids
        .iter()
        .enumerate()
        .map(|(i, t)| ConvergenceDrawCandidate {
            txid: t.repeat(32),
            weight: weights[i],
            res_program: "11".repeat(32),
            sigma: None,
            eligible: replay.candidates[i].draw_eligible,
        })
        .collect();
    let winner = if replay.winner_index >= 0 {
        let i = replay.winner_index as usize;
        Some(ConvergenceDrawWinner {
            index: i as u32,
            txid: txids[i].repeat(32),
            weight: weights[i],
            res_program: "11".repeat(32),
        })
    } else {
        None
    };
    ConvergenceDraw {
        source_blockhash: prev.clone(),
        source_height: height.saturating_sub(1),
        sigma_seed: prev,
        payout_height: height,
        convergence_root: native_to_display_hex(&replay.convergence_root),
        target_prev: "1c00ff00".into(),
        res_probability_target: hex::encode(RES_PROBABILITY_TARGET),
        eligible_root: "dd".repeat(32),
        candidates,
        winner,
    }
}

fn fixture_template() -> rigor_gbt::TemplateRecord {
    let txid = "aa".repeat(32);
    rigor_gbt::build_template_record(
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "ab".repeat(32),
            height: 300,
            longpollid: "lp".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: None,
            coinbasevalue: 500_000_000,
            transactions: vec![GbtTransaction {
                data: "01".into(),
                txid: txid.clone(),
                hash: None,
                depends: None,
                fee: Some(500_000_000),
                sigops: None,
                weight: None,
            }],
            // This block's committed set (for the *next* payout) — independent of the paid draw.
            convergence_txids: vec![txid.clone()],
            convergence_root: None,
            convergence_draw: Some(parent_draw(&["aa"], 300)),
            default_witness_commitment: None,
            default_convergence_commitment: None,
                default_res_mint: None,
                default_pcorb_carrier: None,
            weightlimit: None,
            version: None,
            gbt_payload_hash: [0u8; 32],
            raw_json: "{}".into(),
        },
        None,
        TemplateVersion::INITIAL,
        rigor_gbt::TemplateState::Active,
    )
}

#[test]
fn preview_authorize_invalidate_flow() {
    let tpl = fixture_template();
    let job = fixture_job(&tpl);

    let mut runtime = WinnerRuntime::memory();
    let preview = runtime.preview_winner(&tpl, &job).unwrap();
    assert!(preview.mint_required);

    runtime.register_published_job(&tpl, &job).unwrap();
    let token = runtime
        .authorize_mint_for_block(&tpl, &job, /*treasury_available=*/ true, /*paused=*/ false)
        .unwrap()
        .expect("mint authorization");
    assert_ne!(token.bind_hash, [0u8; 32]);

    runtime
        .invalidate_job(&job, RevocationReason::Reorg)
        .unwrap();
}
