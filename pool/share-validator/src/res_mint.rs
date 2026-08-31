use block_assembler::PublishedJobSnapshot;
use rigor_gbt::{ConvergenceDraw, GbtTransaction, TemplateRecord};

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use crate::txwire::parse_tx;

/// Consensus-locked 100.00 RES = 100 * RES_COIN (RES_COIN = 100 atoms).
/// Copied here for pool-side verification only — this crate does not mint.
pub const RES_REWARD_ATOMS: i64 = 10_000;

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

/// Positive check of the **node-supplied** coinbase mint published on GBT.
///
/// The pool does not construct, modify, or redirect this output. When the
/// parent draw has a winner this template must carry:
/// - `default_res_mint`: W(h) paying the winner's declared program (`OP_RES_MINT`)
/// - `default_pcorb_carrier`: tagged `OP_RETURN` (`PCOR` ‖ 116 fields)
///
/// Body transactions must not create RES or carry the tag. When there is no
/// winner, both GBT fields must be absent.
pub fn validate_node_supplied_automint(template: &TemplateRecord) -> ValidatorResult<()> {
    if gbt_body_carries_res_or_pcorb(&template.gbt.transactions) {
        return Err(res_err(
            "GBT body transactions must not create RES or carry a PCORB tag; mint lives in the coinbase",
        ));
    }
    let winner = template
        .gbt
        .convergence_draw
        .as_ref()
        .and_then(|d| d.winner.as_ref());

    match (
        winner,
        template.gbt.default_res_mint.as_ref(),
        template.gbt.default_pcorb_carrier.as_ref(),
    ) {
        (None, None, None) => Ok(()),
        (None, _, _) => Err(res_err(
            "GBT publishes a coinbase RES mint but convergence_draw has no winner",
        )),
        (Some(_), None, _) | (Some(_), _, None) => Err(res_err(
            "parent draw has a winner but GBT is missing default_res_mint / default_pcorb_carrier",
        )),
        (Some(w), Some(mint), Some(carrier)) => {
            let draw = template
                .gbt
                .convergence_draw
                .as_ref()
                .expect("winner implies draw");
            check_gbt_coinbase_mint(mint, carrier, template, draw, w)
        }
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

    if let Some(ref carrier) = template.gbt.default_pcorb_carrier {
        let needle = carrier.trim().to_ascii_lowercase();
        if needle.is_empty() || !block_hex.to_ascii_lowercase().contains(&needle) {
            return Err(res_err(
                "submitted block does not contain the node GBT PCORB carrier (pool must not drop it)",
            ));
        }
    }
    Ok(())
}

fn gbt_body_carries_res_or_pcorb(txs: &[GbtTransaction]) -> bool {
    txs.iter().any(|gbt_tx| {
        let Ok(raw) = hex::decode(gbt_tx.data.trim()) else {
            return false;
        };
        let Ok(tx) = parse_tx(&raw) else {
            return false;
        };
        tx.vout.iter().any(|o| {
            is_res_script(&o.script) || is_res_mint_script(&o.script) || is_pcorb_carrier(&o.script)
        })
    })
}

fn check_gbt_coinbase_mint(
    mint: &rigor_gbt::GbtResMint,
    carrier_hex: &str,
    template: &TemplateRecord,
    draw: &ConvergenceDraw,
    winner: &rigor_gbt::ConvergenceDrawWinner,
) -> ValidatorResult<()> {
    if mint.value != RES_REWARD_ATOMS {
        return Err(res_err(format!(
            "coinbase RES mint amount is {} atoms; required {RES_REWARD_ATOMS} (100.00 RES)",
            mint.value
        )));
    }
    let spk = hex::decode(mint.script_pubkey.trim())
        .map_err(|e| res_err(format!("default_res_mint scriptPubKey: {e}")))?;
    if !is_res_mint_script(&spk) {
        return Err(res_err("default_res_mint is not an OP_RES_MINT script"));
    }
    let carrier = hex::decode(carrier_hex.trim())
        .map_err(|e| res_err(format!("default_pcorb_carrier: {e}")))?;
    let fields = pcorb_fields_from_carrier(&carrier)?;
    check_pcorb_fields(&fields, template, draw, winner)
}

fn check_pcorb_fields(
    fields: &[u8],
    template: &TemplateRecord,
    draw: &ConvergenceDraw,
    winner: &rigor_gbt::ConvergenceDrawWinner,
) -> ValidatorResult<()> {
    if fields.len() != 116 {
        return Err(res_err("automint PCORB field sizes are invalid"));
    }

    let named = display_hex_to_native32(&winner.txid)?;
    if &fields[0..32] != named.as_slice() {
        return Err(res_err(
            "automint PCORB convergence_txid is not the declared winner",
        ));
    }
    let amt = u64::from_le_bytes(fields[32..40].try_into().unwrap());
    if amt != winner.weight {
        return Err(res_err(format!(
            "automint PCORB amount {amt} does not match winner weight {}",
            winner.weight
        )));
    }
    let bind = display_hex_to_native32(&draw.source_blockhash)?;
    if &fields[40..72] != bind.as_slice() {
        return Err(res_err(
            "automint PCORB bind_hash is not the parent block hash",
        ));
    }
    let height = u64::from_le_bytes(fields[72..80].try_into().unwrap());
    if height != template.height {
        return Err(res_err(format!(
            "automint PCORB height {height} does not match template height {}",
            template.height
        )));
    }
    let nbits = parse_nbits(&template.gbt.prev_nbits)?;
    if fields[80..84] != nbits.to_le_bytes() {
        return Err(res_err(
            "automint PCORB target_prev does not match template prev_nbits",
        ));
    }
    let root = display_hex_to_native32(&draw.convergence_root)?;
    if &fields[84..116] != root.as_slice() {
        return Err(res_err(
            "automint PCORB convergence_root does not match the parent draw C",
        ));
    }
    Ok(())
}

/// C++ `IsResScript`: native v1 / 32 (not CVG2, not RES2), or canonical `OP_RES_MINT`.
pub fn is_res_script(spk: &[u8]) -> bool {
    if is_res_mint_script(spk) {
        return true;
    }
    is_witness_v1_32(spk) && &spk[2..6] != b"CVG2" && &spk[2..6] != b"RES2"
}

fn is_res_mint_script(spk: &[u8]) -> bool {
    // Canonical lock: <CScriptNum maturity> <20-byte HASH160> OP_RES_MINT (0xbb).
    const OP_RES_MINT: u8 = 0xbb;
    const OP_PUSH20: u8 = 20;
    if spk.len() < 22 || spk[spk.len() - 1] != OP_RES_MINT {
        return false;
    }
    if spk[spk.len() - 22] != OP_PUSH20 {
        return false;
    }
    true
}

fn is_pcorb_carrier(spk: &[u8]) -> bool {
    spk.len() == 123 && spk[0] == 0x6a && spk[1] == 0x4c && spk[2] == 0x78 && &spk[3..7] == b"PCOR"
}

fn pcorb_fields_from_carrier(spk: &[u8]) -> ValidatorResult<&[u8]> {
    if !is_pcorb_carrier(spk) {
        return Err(res_err("default_pcorb_carrier is not the canonical tagged OP_RETURN"));
    }
    Ok(&spk[7..])
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

#[cfg(test)]
fn witness_v1_res_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut s = Vec::with_capacity(34);
    s.push(0x51);
    s.push(0x20);
    s.extend_from_slice(program32);
    s
}

#[allow(dead_code)]
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
    use crate::txwire::write_varint;
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

    fn encode_carrier(winner_txid: &str, height: u64) -> String {
        let mut payload = Vec::new();
        payload.extend_from_slice(b"PCOR");
        payload.extend_from_slice(&display_hex_to_native32(winner_txid).unwrap());
        payload.extend_from_slice(&50_000_000u64.to_le_bytes());
        payload.extend_from_slice(&display_hex_to_native32(PARENT).unwrap());
        payload.extend_from_slice(&height.to_le_bytes());
        payload.extend_from_slice(&u32::from_str_radix(PREV_NBITS, 16).unwrap().to_le_bytes());
        payload.extend_from_slice(&display_hex_to_native32(ROOT).unwrap());
        assert_eq!(payload.len(), 120);
        let mut spk = vec![0x6a, 0x4c, 0x78];
        spk.extend_from_slice(&payload);
        hex::encode(spk)
    }

    fn dummy_mint_spk() -> String {
        let mut spk = vec![0x01, 0x14];
        spk.extend(std::iter::repeat_n(0u8, 20));
        spk.push(0xbb);
        hex::encode(spk)
    }

    fn attach_coinbase_mint(tpl: &mut TemplateRecord, amount: i64, winner_txid: &str) {
        tpl.gbt.default_res_mint = Some(rigor_gbt::GbtResMint {
            value: amount,
            script_pubkey: dummy_mint_spk(),
        });
        tpl.gbt.default_pcorb_carrier = Some(encode_carrier(winner_txid, tpl.height));
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
                default_res_mint: None,
                default_pcorb_carrier: None,
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
    fn accepts_node_coinbase_mint() {
        let mut tpl = template(vec![], true);
        attach_coinbase_mint(&mut tpl, RES_REWARD_ATOMS, WINNER_TXID);
        validate_node_supplied_automint(&tpl).unwrap();
    }

    #[test]
    fn rejects_wrong_mint_amount() {
        let mut tpl = template(vec![], true);
        attach_coinbase_mint(&mut tpl, 20_000, WINNER_TXID);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "100.00 RES");
    }

    #[test]
    fn rejects_missing_mint_when_winner() {
        let tpl = template(vec![], true);
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "missing default_res_mint");
    }

    #[test]
    fn rejects_body_mint() {
        let mut tpl = template(vec![], true);
        attach_coinbase_mint(&mut tpl, RES_REWARD_ATOMS, WINNER_TXID);
        // A body RES output is never allowed, even when the coinbase mint is present.
        let mut raw = Vec::new();
        raw.extend_from_slice(&2i32.to_le_bytes());
        write_varint(&mut raw, 1);
        raw.extend_from_slice(&[0x44u8; 32]);
        raw.extend_from_slice(&1u32.to_le_bytes());
        write_varint(&mut raw, 0);
        raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
        write_varint(&mut raw, 1);
        raw.extend_from_slice(&RES_REWARD_ATOMS.to_le_bytes());
        let spk = witness_v1_res_script(&[0x11; 32]);
        write_varint(&mut raw, spk.len());
        raw.extend_from_slice(&spk);
        raw.extend_from_slice(&0u32.to_le_bytes());
        tpl.gbt.transactions.push(mint_tx(hex::encode(raw)));
        let err = validate_node_supplied_automint(&tpl).unwrap_err();
        is_res_err(&err, "body transactions");
    }

    #[test]
    fn rejects_mint_without_winner() {
        let mut tpl = template(vec![], false);
        attach_coinbase_mint(&mut tpl, RES_REWARD_ATOMS, WINNER_TXID);
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
    fn block_must_carry_the_gbt_carrier() {
        let mut tpl = template(vec![], true);
        attach_coinbase_mint(&mut tpl, RES_REWARD_ATOMS, WINNER_TXID);
        let job = dummy_job();
        let carrier = tpl.gbt.default_pcorb_carrier.clone().unwrap();
        let mut block = vec![0u8; 80];
        block.push(0x01);
        block.extend_from_slice(&hex::decode(&carrier).unwrap());
        validate_block_res_mint(&job, &tpl, &hex::encode(&block), true).unwrap();

        let mut empty = vec![0u8; 80];
        empty.push(0x01);
        empty.extend_from_slice(&[0u8; 10]);
        let err = validate_block_res_mint(&job, &tpl, &hex::encode(&empty), true).unwrap_err();
        is_res_err(&err, "PCORB carrier");
    }

    #[test]
    fn p0_018_res_script_matches_classify() {
        let mut native = vec![0x51, 0x20];
        native.extend_from_slice(&[0x11u8; 32]);
        assert!(is_res_script(&native));

        let mut res2 = vec![0x51, 0x20];
        res2.extend_from_slice(b"RES2");
        res2.extend_from_slice(&[0u8; 28]);
        assert!(!is_res_script(&res2));

        let mut cvg2 = vec![0x51, 0x20];
        cvg2.extend_from_slice(b"CVG2");
        cvg2.extend_from_slice(&[0u8; 28]);
        assert!(!is_res_script(&cvg2));

        let mut mint = vec![0x01, 100, 20];
        mint.extend_from_slice(&[0x55u8; 20]);
        mint.push(0xbb);
        assert!(is_res_script(&mint));
    }
}
