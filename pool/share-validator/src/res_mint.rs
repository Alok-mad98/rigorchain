use block_assembler::PublishedJobSnapshot;
use rigor_gbt::{ConvergenceDraw, GbtTransaction, TemplateRecord};
use sha2::{Digest, Sha256};

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use crate::txwire::{parse_tx, ParsedTx};

/// Consensus-locked 100.00 RES = 100 * RES_COIN (RES_COIN = 100 atoms).
/// Copied here for pool-side verification only — this crate does not mint.
pub const RES_REWARD_ATOMS: i64 = 10_000;

/// Six PCORB proof pushes; the automint P2WSH redeem is a seventh trailing item.
const PCORB_WITNESS_ITEMS: usize = 6;

/// ORU-only share gate (TASK-POOL-RES-BOUNDARY-IMPLEMENT-003).
///
/// Jobs must not reserve a RES merkle placeholder or mark mint_required. The pool
/// never synthesizes RES body mints; any mint in GBT remains opaque node data.
pub fn validate_res_mint_placeholder(job: &PublishedJobSnapshot) -> ValidatorResult<()> {
    if job.body_mint.reserved {
        return Err(res_err(
            "ORU-only pool: body mint merkle placeholder must not be reserved",
        ));
    }
    if job.body_mint.mint_required {
        return Err(res_err("ORU-only pool: job must not mark mint_required"));
    }
    if job.body_mint.body_mint_txid.is_some() {
        return Err(res_err(
            "ORU-only pool: job must not carry a pool-built body_mint_txid",
        ));
    }
    Ok(())
}

/// Positive check of the **node-supplied** automint in GBT `transactions`.
///
/// The pool does not construct, modify, or redirect this transaction. When the
/// parent draw has a winner this template must carry exactly one RES mint that:
/// - pays `RES_REWARD_AMOUNT` (100.00 RES) to `OP_1 <winner.res_program>`
/// - spends the AutoResMintBootstrap P2WSH (redeem script in the witness)
/// - returns ORU change to that same bootstrap script
/// - carries a well-formed PCORB witness bound to this height / parent / winner
///
/// When there is no winner, a RES mint in the body is rejected.
pub fn validate_node_supplied_automint(template: &TemplateRecord) -> ValidatorResult<()> {
    let mints = collect_res_mints(&template.gbt.transactions)?;
    let winner = template
        .gbt
        .convergence_draw
        .as_ref()
        .and_then(|d| d.winner.as_ref());

    match (winner, mints.len()) {
        (None, 0) => Ok(()),
        (None, n) => Err(res_err(format!(
            "GBT contains {n} RES mint tx(s) but convergence_draw has no winner"
        ))),
        (Some(_), 0) => Err(res_err(
            "parent draw has a winner but GBT is missing the node automatic RES mint",
        )),
        (Some(_), n) if n > 1 => Err(res_err(format!(
            "GBT contains {n} RES mint txs; consensus allows exactly one"
        ))),
        (Some(w), 1) => {
            let draw = template.gbt.convergence_draw.as_ref().expect("winner implies draw");
            check_automint_shape(&mints[0], template, draw, w)
        }
        _ => Ok(()),
    }
}

/// Block hex advisory check: pool must not have attached a synthesized mint leaf
/// beyond the opaque GBT package already committed in the job merkle.
///
/// When a node automint is in the template, the submitted block must include that
/// same `data` blob (the pool must not replace or redirect it).
pub fn validate_block_res_mint(
    job: &PublishedJobSnapshot,
    template: &TemplateRecord,
    block_hex: &str,
    allow_res_body_mint: bool,
) -> ValidatorResult<()> {
    let _ = allow_res_body_mint;

    if let Some(ref mint_txid) = job.body_mint.body_mint_txid {
        let lower = block_hex.to_ascii_lowercase();
        if lower.contains(&mint_txid.to_ascii_lowercase()) {
            return Err(res_err(
                "unauthorized pool-built RES body mint tx in block submission",
            ));
        }
    }

    validate_node_supplied_automint(template)?;

    let block = hex::decode(block_hex).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
    })?;
    let vtx_count = count_block_transactions(&block)?;
    let expected_max = job.body_mint.slot_index;
    if vtx_count > expected_max {
        return Err(res_err(format!(
            "block has {vtx_count} txs; ORU-only max is {expected_max} (coinbase + opaque GBT)"
        )));
    }

    if let Some(raw) = first_res_mint_data(&template.gbt.transactions)? {
        let needle = raw.trim().to_ascii_lowercase();
        if needle.is_empty() || !block_hex.to_ascii_lowercase().contains(&needle) {
            return Err(res_err(
                "submitted block does not contain the node GBT automint (pool must not replace it)",
            ));
        }
    }
    Ok(())
}

struct LocatedMint {
    tx: ParsedTx,
}

fn collect_res_mints(txs: &[GbtTransaction]) -> ValidatorResult<Vec<LocatedMint>> {
    let mut out = Vec::new();
    for gbt_tx in txs {
        let Ok(raw) = hex::decode(gbt_tx.data.trim()) else {
            continue;
        };
        let Ok(tx) = parse_tx(&raw) else {
            continue;
        };
        if tx.vout.iter().any(|o| is_res_script(&o.script)) {
            out.push(LocatedMint { tx });
        }
    }
    Ok(out)
}

fn first_res_mint_data(txs: &[GbtTransaction]) -> ValidatorResult<Option<String>> {
    for gbt_tx in txs {
        let Ok(raw) = hex::decode(gbt_tx.data.trim()) else {
            continue;
        };
        let Ok(tx) = parse_tx(&raw) else {
            continue;
        };
        if tx.vout.iter().any(|o| is_res_script(&o.script)) {
            return Ok(Some(gbt_tx.data.clone()));
        }
    }
    Ok(None)
}

fn check_automint_shape(
    mint: &LocatedMint,
    template: &TemplateRecord,
    draw: &ConvergenceDraw,
    winner: &rigor_gbt::ConvergenceDrawWinner,
) -> ValidatorResult<()> {
    let tx = &mint.tx;
    if tx.version != 2 {
        return Err(res_err(format!(
            "automint nVersion is {} (node path uses 2)",
            tx.version
        )));
    }
    if tx.vin.len() != 1 {
        return Err(res_err(format!(
            "automint must spend exactly one bootstrap UTXO, got {} inputs",
            tx.vin.len()
        )));
    }
    if tx.vout.len() != 2 {
        return Err(res_err(format!(
            "automint must have RES payout + ORU change, got {} outputs",
            tx.vout.len()
        )));
    }

    let program = decode_program32(&winner.res_program)?;
    let expected_spk = witness_v1_res_script(&program);

    let mut res_outs = 0usize;
    let mut res_total = 0i64;
    let mut change_ok = false;
    for o in &tx.vout {
        if is_res_script(&o.script) {
            res_outs += 1;
            res_total += o.value;
            if o.script != expected_spk {
                return Err(res_err(
                    "automint res_program does not match the winner RESO declaration",
                ));
            }
        } else if o.script == bootstrap_p2wsh_scriptpubkey() {
            if o.value <= 0 {
                return Err(res_err("automint ORU change must be positive"));
            }
            change_ok = true;
        } else {
            return Err(res_err(
                "automint has a non-bootstrap, non-RES output (pool must not redirect mint)",
            ));
        }
    }
    if res_outs != 1 {
        return Err(res_err(format!(
            "automint must have exactly one RES output, got {res_outs}"
        )));
    }
    if res_total != RES_REWARD_ATOMS {
        return Err(res_err(format!(
            "automint RES amount is {res_total} atoms; required {RES_REWARD_ATOMS} (100.00 RES)"
        )));
    }
    if !change_ok {
        return Err(res_err(
            "automint does not return ORU change to the AutoResMintBootstrap P2WSH",
        ));
    }

    let wit = tx.witnesses.get(0).ok_or_else(|| res_err("automint missing witness"))?;
    if wit.len() != PCORB_WITNESS_ITEMS + 1 {
        return Err(res_err(format!(
            "automint witness has {} items; node path uses {} PCORB + bootstrap redeem",
            wit.len(),
            PCORB_WITNESS_ITEMS
        )));
    }
    if wit[PCORB_WITNESS_ITEMS] != bootstrap_redeem_script() {
        return Err(res_err(
            "automint does not spend the AutoResMintBootstrap funding UTXO (redeem mismatch)",
        ));
    }
            check_pcorb_fields(wit, template, draw, winner)?;
            Ok(())
}

fn check_pcorb_fields(
    wit: &[Vec<u8>],
    template: &TemplateRecord,
    draw: &ConvergenceDraw,
    winner: &rigor_gbt::ConvergenceDrawWinner,
) -> ValidatorResult<()> {
    if wit[0].len() != 32 || wit[1].len() != 8 || wit[2].len() != 32 || wit[3].len() != 8
        || wit[4].len() != 4 || wit[5].len() != 32
    {
        return Err(res_err("automint PCORB witness field sizes are invalid"));
    }

    let named = display_hex_to_native32(&winner.txid)?;
    if wit[0].as_slice() != named.as_slice() {
        return Err(res_err(
            "automint PCORB convergence_txid is not the declared winner",
        ));
    }
    let amt = u64::from_le_bytes(wit[1].as_slice().try_into().unwrap());
    if amt != winner.weight {
        return Err(res_err(format!(
            "automint PCORB amount {amt} does not match winner weight {}",
            winner.weight
        )));
    }
    let bind = display_hex_to_native32(&draw.source_blockhash)?;
    if wit[2].as_slice() != bind.as_slice() {
        return Err(res_err(
            "automint PCORB bind_hash is not the parent block hash",
        ));
    }
    let height = u64::from_le_bytes(wit[3].as_slice().try_into().unwrap());
    if height != template.height {
        return Err(res_err(format!(
            "automint PCORB height {height} does not match template height {}",
            template.height
        )));
    }
    let nbits = parse_nbits(&template.gbt.prev_nbits)?;
    if wit[4].as_slice() != nbits.to_le_bytes() {
        return Err(res_err(
            "automint PCORB target_prev does not match template prev_nbits",
        ));
    }
    let root = display_hex_to_native32(&draw.convergence_root)?;
    if wit[5].as_slice() != root.as_slice() {
        return Err(res_err(
            "automint PCORB convergence_root does not match the parent draw C",
        ));
    }
    Ok(())
}

/// Witness v1 RES (`IsResScript`): version 1, 32-byte program, not CVG2.
pub fn is_res_script(spk: &[u8]) -> bool {
    is_witness_v1_32(spk) && &spk[2..6] != b"CVG2"
}

/// Witness v1 CVG2 convergence output (ORU-domain burn, not a RES mint).
pub fn is_cvg2_script(spk: &[u8]) -> bool {
    is_witness_v1_32(spk) && &spk[2..6] == b"CVG2"
}

fn is_witness_v1_32(spk: &[u8]) -> bool {
    spk.len() == 34 && spk[0] == 0x51 && spk[1] == 0x20
}

pub fn body_cvg2_txids(template: &TemplateRecord) -> Vec<String> {
    let mut out = Vec::new();
    for tx in &template.gbt.transactions {
        let Ok(raw) = hex::decode(tx.data.trim()) else {
            continue;
        };
        let Ok(parsed) = parse_tx(&raw) else {
            continue;
        };
        if parsed.vout.iter().any(|o| is_cvg2_script(&o.script)) {
            out.push(tx.txid.clone());
        }
    }
    out
}

fn witness_v1_res_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut s = Vec::with_capacity(34);
    s.push(0x51);
    s.push(0x20);
    s.extend_from_slice(program32);
    s
}

fn bootstrap_redeem_script() -> Vec<u8> {
    let mut s = vec![0x75; PCORB_WITNESS_ITEMS];
    s.push(0x51);
    s
}

fn bootstrap_p2wsh_scriptpubkey() -> Vec<u8> {
    let hash = Sha256::digest(bootstrap_redeem_script());
    let mut s = Vec::with_capacity(34);
    s.push(0x00);
    s.push(0x20);
    s.extend_from_slice(&hash);
    s
}

fn decode_program32(hex_str: &str) -> ValidatorResult<[u8; 32]> {
    let bytes = hex::decode(hex_str.trim()).map_err(|e| res_err(format!("res_program hex: {e}")))?;
    if bytes.len() != 32 {
        return Err(res_err(format!(
            "winner res_program must be 32 bytes, got {}",
            bytes.len()
        )));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

fn display_hex_to_native32(s: &str) -> ValidatorResult<[u8; 32]> {
    let mut bytes = hex::decode(s.trim()).map_err(|e| res_err(format!("txid/hash hex: {e}")))?;
    if bytes.len() != 32 {
        return Err(res_err("expected 32-byte display hex"));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

fn parse_nbits(bits_hex: &str) -> ValidatorResult<u32> {
    u32::from_str_radix(bits_hex.trim(), 16).map_err(|e| res_err(format!("prev_nbits: {e}")))
}

fn count_block_transactions(block: &[u8]) -> ValidatorResult<usize> {
    if block.len() < 84 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "block too short",
        ));
    }
    let mut off = 80usize;
    let tx_count = crate::txwire::read_varint(block, &mut off)?;
    Ok(tx_count as usize)
}

fn res_err(message: impl Into<String>) -> ValidatorError {
    ValidatorError::validation(ValidationErrorCode::ErrResMintInvalid, message)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::txwire::{write_varint, write_witness_stack};
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{
        ConvergenceDrawCandidate, ConvergenceDrawWinner, GbtRecord, GbtTransaction, RecordId,
        TemplateVersion, TipHash,
    };

    const WINNER_TXID: &str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const PARENT: &str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    const ROOT: &str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    const PROGRAM: &str = "1111111111111111111111111111111111111111111111111111111111111111";
    const PREV_NBITS: &str = "1c00ff00";

    fn encode_automint(amount: i64, program_hex: &str, winner_txid: &str, height: u64) -> String {
        let program = decode_program32(program_hex).unwrap();
        let mut wit = Vec::new();
        wit.push(display_hex_to_native32(winner_txid).unwrap().to_vec());
        wit.push(50_000_000u64.to_le_bytes().to_vec());
        wit.push(display_hex_to_native32(PARENT).unwrap().to_vec());
        wit.push(height.to_le_bytes().to_vec());
        wit.push(u32::from_str_radix(PREV_NBITS, 16).unwrap().to_le_bytes().to_vec());
        wit.push(display_hex_to_native32(ROOT).unwrap().to_vec());
        wit.push(bootstrap_redeem_script());

        let mut raw = Vec::new();
        raw.extend_from_slice(&2i32.to_le_bytes());
        raw.push(0x00);
        raw.push(0x01);
        write_varint(&mut raw, 1);
        raw.extend_from_slice(&[0x44u8; 32]);
        raw.extend_from_slice(&1u32.to_le_bytes());
        write_varint(&mut raw, 0);
        raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
        write_varint(&mut raw, 2);
        raw.extend_from_slice(&amount.to_le_bytes());
        let spk = witness_v1_res_script(&program);
        write_varint(&mut raw, spk.len());
        raw.extend_from_slice(&spk);
        let change = 99_999_000i64;
        raw.extend_from_slice(&change.to_le_bytes());
        let boot = bootstrap_p2wsh_scriptpubkey();
        write_varint(&mut raw, boot.len());
        raw.extend_from_slice(&boot);
        write_witness_stack(&mut raw, &wit);
        raw.extend_from_slice(&0u32.to_le_bytes());
        hex::encode(raw)
    }

    fn draw(winner: bool) -> ConvergenceDraw {
        ConvergenceDraw {
            source_blockhash: PARENT.into(),
            source_height: 305,
            sigma_seed: PARENT.into(),
            payout_height: 306,
            convergence_root: ROOT.into(),
            target_prev: PREV_NBITS.into(),
            res_probability_target: "00".repeat(32),
            eligible_root: "00".repeat(32),
            candidates: vec![ConvergenceDrawCandidate {
                txid: WINNER_TXID.into(),
                weight: 50_000_000,
                res_program: PROGRAM.into(),
                sigma: None,
                eligible: true,
            }],
            winner: winner.then_some(ConvergenceDrawWinner {
                index: 0,
                txid: WINNER_TXID.into(),
                weight: 50_000_000,
                res_program: PROGRAM.into(),
            }),
        }
    }

    fn template(txs: Vec<GbtTransaction>, winner: bool) -> TemplateRecord {
        TemplateRecord {
            record_id: RecordId::new_v4(),
            parent_record_id: None,
            template_version: TemplateVersion::INITIAL,
            tip_hash: TipHash(PARENT.into()),
            previousblockhash: PARENT.into(),
            longpollid: "lp".into(),
            height: 306,
            template_state: rigor_gbt::TemplateState::Active,
            gbt_payload_hash: [0u8; 32],
            mint_required: None,
            winner_job_id: None,
            job_id: None,
            candidate_block_id: None,
            created_at: 0,
            state_changed_at: 0,
            gbt: GbtRecord {
                record_id: RecordId::new_v4(),
                fetched_at: 0,
                tip_hash: TipHash(PARENT.into()),
                previousblockhash: PARENT.into(),
                height: 306,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: PREV_NBITS.into(),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 50,
                transactions: txs,
                convergence_txids: vec![WINNER_TXID.into()],
                convergence_root: None,
                convergence_draw: Some(draw(winner)),
                default_witness_commitment: None,
                default_convergence_commitment: None,
                weightlimit: Some(1_000_000),
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
            },
            job_snapshot: None,
        }
    }

    fn mint_tx(data: String) -> GbtTransaction {
        GbtTransaction {
            data,
            txid: "cc".repeat(32),
            hash: None,
            depends: None,
            fee: None,
            sigops: None,
            weight: Some(681),
        }
    }

    fn dummy_job() -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id: JobId("1".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash(PARENT.into()),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: PARENT.into(),
            height: 306,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01".into(),
                coinb2_hex: "02".into(),
                extranonce2_size: 4,
                scriptsig_hex: "03".into(),
                convergence_root_hex: "00".repeat(64),
                payout_script_hex: "76".into(),
                coinbase_value: 50,
            },
            commitments: CommitmentInfo {
                cvge_script_hex: None,
                rigor_script_hex: "6a08".into(),
                witness_commitment_script_hex: None,
                prev_nbits_hex: PREV_NBITS.into(),
            },
            body_mint: BodyMintPlaceholder {
                slot_index: 2,
                reserved: false,
                mint_required: false,
                body_mint_txid: None,
                placeholder_txid: String::new(),
            },
            merkle_branch: vec![],
            version: 0x20000000,
            nbits: "207fffff".into(),
            ntime: 0,
            prevhash: PARENT.into(),
            block_target: "207fffff".into(),
            assembly_hash: [0u8; 32],
            created_at: 0,
            state_changed_at: 0,
        }
    }

    fn is_res_err(err: &ValidatorError, needle: &str) {
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrResMintInvalid,
                ..
            }
        ));
        let msg = format!("{err}");
        assert!(msg.contains(needle), "{msg}");
    }

    #[test]
    fn accepts_node_automint_with_historical_window() {
        let data = encode_automint(RES_REWARD_ATOMS, PROGRAM, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data)], true);
        validate_node_supplied_automint(&tpl).unwrap();
    }

    #[test]
    fn rejects_wrong_mint_amount() {
        let data = encode_automint(20_000, PROGRAM, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data)], true);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "100.00 RES");
    }

    #[test]
    fn rejects_wrong_recipient() {
        let other = "2222222222222222222222222222222222222222222222222222222222222222";
        let data = encode_automint(RES_REWARD_ATOMS, other, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data)], true);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "res_program");
    }

    #[test]
    fn rejects_missing_mint_when_winner() {
        let tpl = template(vec![], true);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "missing the node automatic RES mint");
    }

    #[test]
    fn rejects_extra_mint() {
        let data = encode_automint(RES_REWARD_ATOMS, PROGRAM, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data.clone()), mint_tx(data)], true);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "exactly one");
    }

    #[test]
    fn rejects_mint_without_winner() {
        let data = encode_automint(RES_REWARD_ATOMS, PROGRAM, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data)], false);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "no winner");
    }

    #[test]
    fn accepts_no_winner_no_mint() {
        let mut tpl = template(vec![], false);
        tpl.gbt.convergence_draw = None;
        validate_node_supplied_automint(&tpl).unwrap();
    }

    #[test]
    fn placeholder_still_forbids_pool_built_mint() {
        let mut job = dummy_job();
        job.body_mint.mint_required = true;
        assert!(validate_res_mint_placeholder(&job).is_err());
        job.body_mint.mint_required = false;
        job.body_mint.body_mint_txid = Some("ab".repeat(32));
        assert!(validate_res_mint_placeholder(&job).is_err());
    }

    #[test]
    fn block_must_carry_the_gbt_automint() {
        let data = encode_automint(RES_REWARD_ATOMS, PROGRAM, WINNER_TXID, 306);
        let tpl = template(vec![mint_tx(data.clone())], true);
        let job = dummy_job();
        // header (80 zero bytes) + varint(1) is too small a vtx set but we need the mint hex inside.
        let mut block = vec![0u8; 80];
        block.push(0x01);
        block.extend_from_slice(&hex::decode(&data).unwrap());
        validate_block_res_mint(&job, &tpl, &hex::encode(&block), true).unwrap();

        let mut empty = vec![0u8; 80];
        empty.push(0x01);
        empty.extend_from_slice(&[0u8; 10]);
        let err = validate_block_res_mint(&job, &tpl, &hex::encode(&empty), true).unwrap_err();
        is_res_err(&err, "does not contain the node GBT automint");
    }
}
