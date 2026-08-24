use block_assembler::{
    BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    PublishedJobSnapshot,
};
use mint_engine::{
    derive_bind_hash_chain_accurate, derive_bind_hash_infra, MintBlockContext, MintEngine,
    MintEngineConfig, MintWinnerContext, RES_REWARD_ATOMS,
};
use block_assembler::compute_convergence_root;
use rigor_gbt::{
    build_template_record, GbtRecord, GbtTransaction, RecordId, TemplateState, TemplateVersion,
    TipHash,
};

/// Owner program the fixture convergence transactions declare.
const OWNER_PROGRAM: [u8; 32] = [0xa1; 32];

/// Raw single-input transaction carrying the given `(value, scriptPubKey)` outputs.
fn raw_tx_with_outputs(outputs: &[(i64, Vec<u8>)]) -> String {
    let mut raw = Vec::new();
    raw.extend_from_slice(&2i32.to_le_bytes());
    raw.push(0x01); // one input
    raw.extend_from_slice(&[0x11; 32]);
    raw.extend_from_slice(&0u32.to_le_bytes());
    raw.push(0x00); // empty scriptSig
    raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
    raw.push(outputs.len() as u8);
    for (value, script) in outputs {
        raw.extend_from_slice(&value.to_le_bytes());
        raw.push(script.len() as u8);
        raw.extend_from_slice(script);
    }
    raw.extend_from_slice(&0u32.to_le_bytes()); // locktime
    hex::encode(raw)
}

/// The unspendable ORU burn output that makes a transaction a convergence.
fn cvg2_burn_script() -> Vec<u8> {
    let mut script = vec![0x51u8, 0x20, 0x43, 0x56, 0x47, 0x32]; // OP_1 <32> "CVG2"
    script.extend_from_slice(&[0x00; 28]);
    script
}

/// SECURITY-001-A / A-5: a convergence tx only enters `F` if it declares its RES payout owner,
/// and the mint must pay exactly that program, so template entries must be real declaring txs.
fn convergence_tx_declaring(program32: &[u8; 32]) -> String {
    raw_tx_with_outputs(&[
        (100_000_000, cvg2_burn_script()),
        (0, mint_engine::res_owner::res_owner_declaration_script(program32)),
    ])
}

/// A convergence tx that burns ORU but names no RES owner, so it cannot be in `F`.
fn convergence_tx_without_declaration() -> String {
    raw_tx_with_outputs(&[(100_000_000, cvg2_burn_script())])
}

fn declaring_gbt_transactions(txids: &[String]) -> Vec<GbtTransaction> {
    txids
        .iter()
        .map(|txid| GbtTransaction {
            data: convergence_tx_declaring(&OWNER_PROGRAM),
            txid: txid.clone(),
            hash: None,
            depends: None,
            fee: Some(1),
            sigops: None,
            weight: None,
        })
        .collect()
}

fn job(root_hex: &str) -> PublishedJobSnapshot {
    PublishedJobSnapshot {
        job_id: JobId("integration-job".into()),
        candidate_block_id: CandidateBlockId(99),
        record_id: RecordId::new_v4(),
        tip_hash: TipHash("aa".repeat(32)),
        template_version: TemplateVersion::INITIAL,
        previousblockhash: "00".repeat(32),
        height: 400,
        state: JobState::Active,
        coinbase: CoinbaseInfo {
            coinb1_hex: "01000000010000000000".into(),
            coinb2_hex: "ffffffff".into(),
            extranonce2_size: 4,
            scriptsig_hex: "03".into(),
            convergence_root_hex: root_hex.into(),
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
            slot_index: 3,
            reserved: true,
            mint_required: true,
            body_mint_txid: None,
            placeholder_txid: "cc".repeat(32),
        },
        merkle_branch: vec![],
        version: 1,
        nbits: "207fffff".into(),
        ntime: 0,
        prevhash: "00".repeat(32),
        block_target: "207fffff".into(),
        assembly_hash: [0u8; 32],
        created_at: 0,
        state_changed_at: 0,
    }
}

#[test]
fn winning_job_to_candidate_block() {
    let txids = vec!["aa".repeat(32), "bb".repeat(32)];
    let root_hex = block_assembler::native_to_hash_hex(&compute_convergence_root(&txids));
    let template = build_template_record(
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            height: 400,
            longpollid: "int/1".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: None,
            coinbasevalue: 500_000_000,
            transactions: declaring_gbt_transactions(&txids),
            convergence_txids: txids,
            convergence_root: None,
            convergence_draw: None,
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
        TemplateState::Active,
    );

    let engine = MintEngine::new(MintEngineConfig::devtest());
    let preview = MintWinnerContext {
        mint_required: true,
        winner_txid: Some("aa".repeat(32)),
        convergence_root_hex: root_hex.clone(),
        draw_target_prev_nbits: "1c00ff00".into(),
        authorized_bind_hash: Some([0x11; 32]),
        authorized_bind_hash_display: Some("11".repeat(32)),
    };

    let outcome = engine
        .mint_for_winner(
            &preview,
            &template,
            &job(&root_hex),
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 0x65000000,
                nonce: 0xdeadbeef,
            },
        )
        .unwrap()
        .unwrap();

    assert_eq!(outcome.artifact.amount_atoms, RES_REWARD_ATOMS);
    assert_eq!(outcome.block.vtx_count, 4);
    assert!(outcome.block.block_hex.contains(&outcome.artifact.body_mint_txid) == false);
    // block hex is raw bytes; txid is display-endian
    assert!(!outcome.block.block_hex.is_empty());
}

#[test]
fn reorg_revokes_mint_state() {
    let engine = MintEngine::new(MintEngineConfig::devtest());
    let txids = vec!["aa".repeat(32), "bb".repeat(32)];
    let root_hex = block_assembler::native_to_hash_hex(&compute_convergence_root(&txids));
    let template = build_template_record(
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            height: 400,
            longpollid: "int/2".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: None,
            coinbasevalue: 500_000_000,
            transactions: declaring_gbt_transactions(&txids),
            convergence_txids: txids,
            convergence_root: None,
            convergence_draw: None,
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
        TemplateState::Active,
    );

    let preview = MintWinnerContext {
        mint_required: true,
        winner_txid: Some("aa".repeat(32)),
        convergence_root_hex: root_hex.clone(),
        draw_target_prev_nbits: "1c00ff00".into(),
        authorized_bind_hash: Some([0x11; 32]),
        authorized_bind_hash_display: Some("11".repeat(32)),
    };

    engine
        .mint_for_winner(
            &preview,
            &template,
            &job(&root_hex),
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 1,
                nonce: 1,
            },
        )
        .unwrap();

    let revoked = engine
        .revoke_for_jobs(&["integration-job".into()])
        .unwrap();
    assert_eq!(revoked.len(), 1);
}

/// SECURITY-001-A / A-5: the pool no longer has a RES address of its own. The mint must pay the
/// program the winning convergence declared, or consensus rejects the block with
/// `bad-rigor-res-owner-mismatch`.
#[test]
fn mint_pays_the_owner_declared_by_the_winning_convergence() {
    let engine = MintEngine::new(MintEngineConfig::devtest());
    let txids = vec!["aa".repeat(32), "bb".repeat(32)];
    let root_hex = block_assembler::native_to_hash_hex(&compute_convergence_root(&txids));
    let template = build_template_record(
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            height: 400,
            longpollid: "int/3".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: None,
            coinbasevalue: 500_000_000,
            transactions: declaring_gbt_transactions(&txids),
            convergence_txids: txids,
            convergence_root: None,
            convergence_draw: None,
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
        TemplateState::Active,
    );
    let preview = MintWinnerContext {
        mint_required: true,
        winner_txid: Some("aa".repeat(32)),
        convergence_root_hex: root_hex.clone(),
        draw_target_prev_nbits: "1c00ff00".into(),
        authorized_bind_hash: Some([0x11; 32]),
        authorized_bind_hash_display: Some("11".repeat(32)),
    };

    let outcome = engine
        .mint_for_winner(
            &preview,
            &template,
            &job(&root_hex),
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 1,
                nonce: 1,
            },
        )
        .unwrap()
        .expect("mint outcome");

    // vout[0] must be OP_1 <declared program>.
    let mut expected_spk = vec![0x51u8, 0x20];
    expected_spk.extend_from_slice(&OWNER_PROGRAM);
    assert!(
        outcome.artifact.raw_tx_hex.contains(&hex::encode(&expected_spk)),
        "mint does not pay the declared owner program"
    );

    // And must not pay the pool's old devtest placeholder program.
    let mut placeholder_spk = vec![0x51u8, 0x20];
    placeholder_spk.extend_from_slice(&[0x42u8; 32]);
    assert!(
        !outcome.artifact.raw_tx_hex.contains(&hex::encode(&placeholder_spk)),
        "mint still pays a pool-held program"
    );
}

/// A convergence that declares no owner cannot be in `F`; reaching the mint with one means the
/// pool's draw replay and the node's disagree, so it must fail rather than guess a payee.
#[test]
fn mint_refuses_when_the_winner_declares_no_owner() {
    let engine = MintEngine::new(MintEngineConfig::devtest());
    let txids = vec!["aa".repeat(32), "bb".repeat(32)];
    let root_hex = block_assembler::native_to_hash_hex(&compute_convergence_root(&txids));
    let undeclared = GbtTransaction {
        data: convergence_tx_without_declaration(),
        txid: txids[0].clone(),
        hash: None,
        depends: None,
        fee: Some(1),
        sigops: None,
        weight: None,
    };
    let template = build_template_record(
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            height: 400,
            longpollid: "int/4".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: None,
            coinbasevalue: 500_000_000,
            transactions: vec![undeclared],
            convergence_txids: txids,
            convergence_root: None,
            convergence_draw: None,
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
        TemplateState::Active,
    );
    let preview = MintWinnerContext {
        mint_required: true,
        winner_txid: Some("aa".repeat(32)),
        convergence_root_hex: root_hex.clone(),
        draw_target_prev_nbits: "1c00ff00".into(),
        authorized_bind_hash: Some([0x11; 32]),
        authorized_bind_hash_display: Some("11".repeat(32)),
    };

    let err = engine
        .mint_for_winner(
            &preview,
            &template,
            &job(&root_hex),
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 1,
                nonce: 1,
            },
        )
        .map(|_| ())
        .expect_err("mint must not be built for an undeclared winner");
    assert!(format!("{err}").contains("declares no RES owner"), "unexpected error: {err}");
}

#[test]
fn bind_hash_is_deterministic() {
    #[allow(deprecated)]
    let a = derive_bind_hash_infra("aa".repeat(32).as_str(), 100);
    #[allow(deprecated)]
    let b = derive_bind_hash_infra("aa".repeat(32).as_str(), 100);
    assert_eq!(a, b);
}

#[test]
fn chain_accurate_bind_matches_ancestor_at_height_minus_k() {
    let ancestor = "a73c9156d549fa3129c3f936e612289658e38e60307151f37fb1bfa6bb09b7a3";
    let b = derive_bind_hash_chain_accurate(ancestor, 2100001).unwrap();
    assert_eq!(hex::encode(b), ancestor);
    let boot = derive_bind_hash_chain_accurate(ancestor, 5).unwrap();
    assert_eq!(boot, [0u8; 32]);
}
