use block_assembler::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use crate::txwire::parse_tx;

/// Consensus-locked 100.00 RES = 100 * RES_COIN (RES_COIN = 100 atoms).
/// Copied here for leftover script classifiers only — this crate does not mint.
pub const RES_REWARD_ATOMS: i64 = 10_000;

/// ORU-only share gate (RFC-020).
///
/// Jobs must not reserve a RES merkle placeholder or mark mint_required. The pool
/// never synthesizes RES body mints.
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

/// Block hex advisory check: pool must not have attached a synthesized mint leaf
/// beyond the opaque GBT package already committed in the job merkle.
pub fn validate_block_res_mint(
    job: &PublishedJobSnapshot,
    template: &TemplateRecord,
    block_hex: &str,
    allow_res_body_mint: bool,
) -> ValidatorResult<()> {
    let _ = (allow_res_body_mint, template);

    if let Some(ref mint_txid) = job.body_mint.body_mint_txid {
        let lower = block_hex.to_ascii_lowercase();
        if lower.contains(&mint_txid.to_ascii_lowercase()) {
            return Err(res_err(
                "unauthorized pool-built RES body mint tx in block submission",
            ));
        }
    }

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
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{RecordId, TemplateVersion, TipHash};

    const PARENT: &str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    const PREV_NBITS: &str = "1c00ff00";

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
                rigor_script_hex: String::new(),
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
