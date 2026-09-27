//! ORU-only solved-block assembly .
//!
//! Builds a full block hex from a published job + share solution using only:
//! coinbase (with extranonces) + opaque GBT transaction bytes.
//!
//! The pool MUST NOT insert, synthesize, or rewrite RES body-mint transactions.
//! Any RES mint present in the GBT package is treated as opaque node-supplied data.
//!
//! Merkle leaves are Bitcoin **txids** (witness stripped), matching `CheckMerkleRoot`.

use crate::error::{AssemblerError, AssemblerResult};
use crate::merkle::{build_merkle_branches, txid_from_tx_bytes};
use crate::types::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;

/// Share solution fields needed to finalize coinbase + header.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SolvedBlockContext {
    pub extranonce1: Vec<u8>,
    pub extranonce2: Vec<u8>,
    pub ntime: u32,
    pub nonce: u32,
}

/// Assembled block package ready for `submitblock`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SolvedBlockPackage {
    pub block_hex: String,
    pub vtx_count: usize,
    /// Internal-order merkle root written into the block header.
    pub merkle_root: [u8; 32],
}

/// Assemble a found block: `vtx = coinbase ‖ GBT.transactions` (opaque).
///
/// Refuses jobs that still carry a legacy body-mint merkle placeholder reservation
/// (`body_mint.reserved == true`); ORU-only jobs never reserve that slot.
pub fn assemble_solved_block(
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
    ctx: &SolvedBlockContext,
) -> AssemblerResult<SolvedBlockPackage> {
    if job.body_mint.reserved {
        return Err(AssemblerError::Assembly(
            "legacy body-mint merkle placeholder reserved; ORU-only pool cannot assemble this job"
                .into(),
        ));
    }
    if job.body_mint.mint_required {
        return Err(AssemblerError::Assembly(
            "job marked mint_required; ORU-only pool refuses RES mint construction"
                .into(),
        ));
    }

    let coinbase_raw = assemble_coinbase_raw(job, &ctx.extranonce1, &ctx.extranonce2)?;

    let mut vtx: Vec<Vec<u8>> = Vec::with_capacity(1 + template.gbt.transactions.len());
    vtx.push(coinbase_raw);
    for gbt_tx in &template.gbt.transactions {
        let raw = hex::decode(&gbt_tx.data)
            .map_err(|e| AssemblerError::Assembly(format!("gbt tx decode: {e}")))?;
        vtx.push(raw);
    }

    let tx_hashes: Vec<[u8; 32]> = vtx.iter().map(|raw| txid_from_tx_bytes(raw)).collect();
    let (root, _branch) = build_merkle_branches(&tx_hashes);

    let prevhash = decode_prevhash(&job.prevhash)?;
    let nbits = parse_nbits_hex(&job.nbits)?;

    let mut header = [0u8; 80];
    header[0..4].copy_from_slice(&job.version.to_le_bytes());
    header[4..36].copy_from_slice(&prevhash);
    header[36..68].copy_from_slice(&root);
    header[68..72].copy_from_slice(&ctx.ntime.to_le_bytes());
    header[72..76].copy_from_slice(&nbits.to_le_bytes());
    header[76..80].copy_from_slice(&ctx.nonce.to_le_bytes());

    let vtx_count = vtx.len();
    let mut block = Vec::new();
    block.extend_from_slice(&header);
    write_varint(&mut block, vtx_count as u64);
    for tx in vtx {
        block.extend_from_slice(&tx);
    }

    Ok(SolvedBlockPackage {
        block_hex: hex::encode(block),
        vtx_count,
        merkle_root: root,
    })
}

pub fn assemble_coinbase_raw(
    job: &PublishedJobSnapshot,
    extranonce1: &[u8],
    extranonce2: &[u8],
) -> AssemblerResult<Vec<u8>> {
    let coinb1 = hex::decode(&job.coinbase.coinb1_hex)
        .map_err(|e| AssemblerError::Assembly(e.to_string()))?;
    let coinb2 = hex::decode(&job.coinbase.coinb2_hex)
        .map_err(|e| AssemblerError::Assembly(e.to_string()))?;
    if extranonce2.len() != job.coinbase.extranonce2_size as usize {
        return Err(AssemblerError::Assembly(format!(
            "extranonce2 len {} != expected {}",
            extranonce2.len(),
            job.coinbase.extranonce2_size
        )));
    }
    // Jobs reserve a fixed extranonce1 gap (AssemblerConfig default / session = 4).
    let en1 = normalize_extranonce1(extranonce1)?;
    let mut raw =
        Vec::with_capacity(coinb1.len() + en1.len() + extranonce2.len() + coinb2.len());
    raw.extend_from_slice(&coinb1);
    raw.extend_from_slice(&en1);
    raw.extend_from_slice(extranonce2);
    raw.extend_from_slice(&coinb2);
    Ok(raw)
}

/// Pool stratum sessions always use 4-byte extranonce1; left-pad shorter values.
pub fn normalize_extranonce1(extranonce1: &[u8]) -> AssemblerResult<Vec<u8>> {
    const EN1: usize = 4;
    if extranonce1.len() > EN1 {
        return Err(AssemblerError::Assembly(format!(
            "extranonce1 len {} exceeds pool size {EN1}",
            extranonce1.len()
        )));
    }
    let mut out = vec![0u8; EN1 - extranonce1.len()];
    out.extend_from_slice(extranonce1);
    Ok(out)
}

fn write_varint(buf: &mut Vec<u8>, n: u64) {
    if n < 0xfd {
        buf.push(n as u8);
    } else if n <= 0xffff {
        buf.push(0xfd);
        buf.extend_from_slice(&(n as u16).to_le_bytes());
    } else if n <= 0xffffffff {
        buf.push(0xfe);
        buf.extend_from_slice(&(n as u32).to_le_bytes());
    } else {
        buf.push(0xff);
        buf.extend_from_slice(&n.to_le_bytes());
    }
}

fn parse_nbits_hex(hex_str: &str) -> AssemblerResult<u32> {
    u32::from_str_radix(hex_str, 16).map_err(|e| AssemblerError::Assembly(e.to_string()))
}

fn decode_prevhash(prevhash: &str) -> AssemblerResult<[u8; 32]> {
    let mut bytes = hex::decode(prevhash).map_err(|e| AssemblerError::Assembly(e.to_string()))?;
    if bytes.len() != 32 {
        return Err(AssemblerError::Assembly("prevhash must be 32 bytes".into()));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::assembly::{assemble_from_gbt, AssemblerConfig};
    use crate::merkle::txid_from_tx_bytes;
    use crate::types::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{
        build_template_record, GbtRecord, RecordId, TemplateState, TemplateVersion, TipHash,
    };

    fn sample_job(reserved: bool, mint_required: bool) -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id: JobId("job-1".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash("aa".repeat(32)),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: "00".repeat(32),
            height: 300,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01000000010000000000".into(),
                coinb2_hex: "ffffffff".into(),
                extranonce2_size: 4,
                scriptsig_hex: "03".into(),
                convergence_root_hex: "11".repeat(64),
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
                slot_index: 1,
                reserved,
                mint_required,
                body_mint_txid: None,
                placeholder_txid: String::new(),
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

    fn empty_template() -> TemplateRecord {
        let tip = TipHash("aa".repeat(32));
        let gbt = GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: tip.clone(),
            previousblockhash: "00".repeat(32),
            height: 300,
            longpollid: "lp".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: Some(1),
            coinbasevalue: 50,
            transactions: vec![],
            default_witness_commitment: None,
            weightlimit: Some(1_000_000),
            version: Some(1),
            gbt_payload_hash: [0u8; 32],
            raw_json: "{}".into(),
            default_coinbase_scriptsig: None,
            default_coinbase_tx: None,
            default_coinbase_txid: None,
            merkleroot: None,
            rigor_coinbase_immutable: None,
            coinbase_address: None,
        };
        build_template_record(gbt, None, TemplateVersion::INITIAL, TemplateState::Active)
    }

    #[test]
    fn refuses_legacy_reserved_placeholder() {
        let err = assemble_solved_block(
            &empty_template(),
            &sample_job(true, false),
            &SolvedBlockContext {
                extranonce1: vec![1, 0, 0, 0],
                extranonce2: vec![0, 0, 0, 1],
                ntime: 1,
                nonce: 1,
            },
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("placeholder") || err.contains("reserved"), "{err}");
    }

    #[test]
    fn refuses_mint_required_flag() {
        let err = assemble_solved_block(
            &empty_template(),
            &sample_job(false, true),
            &SolvedBlockContext {
                extranonce1: vec![1, 0, 0, 0],
                extranonce2: vec![0, 0, 0, 1],
                ntime: 1,
                nonce: 1,
            },
        )
        .unwrap_err()
        .to_string();
        assert!(err.contains("mint_required"), "{err}");
    }

    #[test]
    fn solved_merkle_root_matches_txid_tree_and_job_commitment() {
        let mut payout = vec![0x00, 0x14];
        payout.extend_from_slice(&[0x11u8; 20]);
        let cfg = AssemblerConfig::with_payout_script(payout).unwrap();

        let tip = TipHash("aa".repeat(32));
        let gbt = GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: tip.clone(),
            previousblockhash: "11".repeat(32),
            height: 50,
            longpollid: "lp".into(),
            bits: "207fffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: None,
            curtime: Some(1_700_000_000),
            coinbasevalue: 12_800_000_000,
            transactions: vec![],
            default_witness_commitment: Some(hex::encode([
                0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed, 0u8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            ])),
            weightlimit: Some(1_000_000),
            version: Some(0x20000000),
            gbt_payload_hash: [0u8; 32],
            raw_json: "{}".into(),
            default_coinbase_scriptsig: None,
            default_coinbase_tx: None,
            default_coinbase_txid: None,
            merkleroot: None,
            rigor_coinbase_immutable: None,
            coinbase_address: None,
        };
        let artifact = assemble_from_gbt(&gbt, &cfg, &[0u8; 16]).unwrap();
        let template =
            build_template_record(gbt, None, TemplateVersion::INITIAL, TemplateState::Active);

        let job = PublishedJobSnapshot {
            job_id: JobId("1".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: template.record_id,
            tip_hash: tip,
            template_version: TemplateVersion::INITIAL,
            previousblockhash: template.gbt.previousblockhash.clone(),
            height: 50,
            state: JobState::Active,
            coinbase: artifact.coinbase_info.clone(),
            commitments: artifact.commitments.clone(),
            body_mint: artifact.body_mint.clone(),
            merkle_branch: artifact.merkle_branch.clone(),
            version: artifact.version,
            nbits: artifact.nbits.clone(),
            ntime: artifact.ntime,
            prevhash: artifact.prevhash.clone(),
            block_target: artifact.block_target.clone(),
            assembly_hash: artifact.assembly_hash,
            created_at: 0,
            state_changed_at: 0,
        };

        let en1 = [0xAAu8, 0xBB, 0xCC, 0xDD];
        let en2 = [0x01u8, 0x02, 0x03, 0x04];
        let pkg = assemble_solved_block(
            &template,
            &job,
            &SolvedBlockContext {
                extranonce1: en1.to_vec(),
                extranonce2: en2.to_vec(),
                ntime: 1_700_000_000,
                nonce: 42,
            },
        )
        .unwrap();

        // Header merkle == txid of solved coinbase (coinbase-only block).
        // Extranonce is in OP_RETURN, so this differs from the placeholder job leaf.
        let coinbase_raw = assemble_coinbase_raw(&job, &en1, &en2).unwrap();
        assert_eq!(pkg.merkle_root, txid_from_tx_bytes(&coinbase_raw));
        assert_ne!(
            pkg.merkle_root, artifact.merkle_root,
            "non-zero extranonce in OP_RETURN must change the txid merkle leaf"
        );

        let block = hex::decode(&pkg.block_hex).unwrap();
        let header_root: [u8; 32] = block[36..68].try_into().unwrap();
        assert_eq!(header_root, pkg.merkle_root);

        // Witness commitment must equal BIP141 H(null_root || zero_reserved).
        let expect_commit =
            crate::merkle::build_witness_commitment_script(&[0u8; 32], &[0u8; 32]);
        assert!(
            block.windows(expect_commit.len()).any(|w| w == expect_commit.as_slice()),
            "solved block missing recomputed witness commitment"
        );

        let mut off = 80usize;
        let _vtx_count = block[off];
        off += 1;
        let coinbase = &block[off..];
        assert_eq!(txid_from_tx_bytes(coinbase), pkg.merkle_root);
        assert_eq!(coinbase[4], 0x00);
        assert_eq!(coinbase[5], 0x01);
    }
}
