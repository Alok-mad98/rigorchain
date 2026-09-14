use block_assembler::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;

use crate::error::{MintError, MintResult};
use crate::sigma::sha256d;
use crate::types::CandidateBlockPackage;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MintBlockContext {
    pub extranonce1: Vec<u8>,
    pub extranonce2: Vec<u8>,
    pub ntime: u32,
    pub nonce: u32,
}

pub fn assemble_coinbase_raw(
    job: &PublishedJobSnapshot,
    extranonce1: &[u8],
    extranonce2: &[u8],
) -> MintResult<Vec<u8>> {
    let coinb1 = hex::decode(&job.coinbase.coinb1_hex)
        .map_err(|e| MintError::Build(e.to_string()))?;
    let coinb2 = hex::decode(&job.coinbase.coinb2_hex)
        .map_err(|e| MintError::Build(e.to_string()))?;
    if extranonce2.len() != job.coinbase.extranonce2_size as usize {
        return Err(MintError::Build(format!(
            "extranonce2 len {} != expected {}",
            extranonce2.len(),
            job.coinbase.extranonce2_size
        )));
    }
    let mut raw = Vec::with_capacity(coinb1.len() + extranonce1.len() + extranonce2.len() + coinb2.len());
    raw.extend_from_slice(&coinb1);
    raw.extend_from_slice(extranonce1);
    raw.extend_from_slice(extranonce2);
    raw.extend_from_slice(&coinb2);
    Ok(raw)
}

fn parse_nbits_hex(hex_str: &str) -> MintResult<u32> {
    u32::from_str_radix(hex_str, 16).map_err(|e| MintError::Build(e.to_string()))
}

fn decode_prevhash(prevhash: &str) -> MintResult<[u8; 32]> {
    let mut bytes = hex::decode(prevhash).map_err(|e| MintError::Build(e.to_string()))?;
    if bytes.len() != 32 {
        return Err(MintError::Build("prevhash must be 32 bytes".into()));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

fn merkle_root(mut hashes: Vec<[u8; 32]>) -> [u8; 32] {
    if hashes.is_empty() {
        return [0u8; 32];
    }
    while hashes.len() > 1 {
        if hashes.len() % 2 == 1 {
            hashes.push(*hashes.last().unwrap());
        }
        let mut next = Vec::with_capacity(hashes.len() / 2);
        for pair in hashes.chunks(2) {
            let mut concat = Vec::with_capacity(64);
            concat.extend_from_slice(&pair[0]);
            concat.extend_from_slice(&pair[1]);
            next.push(sha256d(&concat));
        }
        hashes = next;
    }
    hashes[0]
}

fn txid_for_block_merkle(tx_raw: &[u8]) -> [u8; 32] {
    block_assembler::txid_from_tx_bytes(tx_raw)
}

/// Canonical vtx: coinbase, GBT txs, RES body mint at ``k+1``.
pub fn assemble_candidate_block(
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
    mint_raw_hex: &str,
    body_mint_txid: &str,
    block_ctx: &MintBlockContext,
) -> MintResult<CandidateBlockPackage> {
    let mint_raw = hex::decode(mint_raw_hex).map_err(|e| MintError::Build(e.to_string()))?;
    assemble_block(template, job, Some(mint_raw), body_mint_txid, block_ctx)
}

/// Assemble a found block that carries **no** RES body mint (`vtx = coinbase || GBT txs`).
///
/// C-5: block submission must never depend on whether a RES mint was required or
/// succeeded. Callers must only use this when the job reserved no body-mint slot
/// (`job.body_mint.reserved == false`); otherwise the merkle branch handed to the
/// miner commits to a placeholder leaf and a mint-less block cannot reproduce it.
pub fn assemble_block_without_mint(
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
    block_ctx: &MintBlockContext,
) -> MintResult<CandidateBlockPackage> {
    if job.body_mint.reserved {
        return Err(MintError::Build(
            "job reserved a body-mint slot; a mint-less block cannot satisfy its merkle commitment"
                .into(),
        ));
    }
    assemble_block(template, job, None, "", block_ctx)
}

fn assemble_block(
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
    mint_raw: Option<Vec<u8>>,
    body_mint_txid: &str,
    block_ctx: &MintBlockContext,
) -> MintResult<CandidateBlockPackage> {
    let coinbase_raw = assemble_coinbase_raw(job, &block_ctx.extranonce1, &block_ctx.extranonce2)?;

    let mut vtx: Vec<Vec<u8>> = Vec::with_capacity(2 + template.gbt.transactions.len());
    vtx.push(coinbase_raw);
    for gbt_tx in &template.gbt.transactions {
        let raw = hex::decode(&gbt_tx.data).map_err(|e| MintError::Build(e.to_string()))?;
        vtx.push(raw);
    }
    let has_mint = mint_raw.is_some();
    if let Some(raw) = mint_raw {
        vtx.push(raw);
    }

    let tx_hashes: Vec<[u8; 32]> = vtx.iter().map(|raw| txid_for_block_merkle(raw)).collect();
    let root = merkle_root(tx_hashes);

    let prevhash = decode_prevhash(&job.prevhash)?;
    let nbits = parse_nbits_hex(&job.nbits)?;

    let mut header = [0u8; 80];
    header[0..4].copy_from_slice(&job.version.to_le_bytes());
    header[4..36].copy_from_slice(&prevhash);
    header[36..68].copy_from_slice(&root);
    header[68..72].copy_from_slice(&block_ctx.ntime.to_le_bytes());
    header[72..76].copy_from_slice(&nbits.to_le_bytes());
    header[76..80].copy_from_slice(&block_ctx.nonce.to_le_bytes());

    let vtx_count = vtx.len();
    let mut block = Vec::new();
    block.extend_from_slice(&header);
    write_varint(&mut block, vtx_count as u64);
    for tx in vtx {
        block.extend_from_slice(&tx);
    }

    debug_assert_eq!(
        vtx_count,
        1 + template.gbt.transactions.len() + usize::from(has_mint)
    );
    Ok(CandidateBlockPackage {
        block_hex: hex::encode(block),
        body_mint_txid: body_mint_txid.to_string(),
        vtx_count,
    })
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

#[cfg(test)]
mod tests {
    use super::*;
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TemplateVersion, TipHash};
    use crate::txbuild::{build_body_mint_tx, witness_v1_res_script, bootstrap_p2wsh_scriptpubkey};
    use crate::types::{ConvergeProofFields, MintBuildParams, MintJobRef, OruFundingInput};
    use crate::sigma::display_hex_to_le32;

    fn sample_job() -> PublishedJobSnapshot {
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
    fn mint_inserted_at_k_plus_one() {
        let program = [0x22u8; 32];
        let artifact = build_body_mint_tx(&MintBuildParams {
            mint_job: MintJobRef {
                winner_job_id: None,
                job_id: "job-1".into(),
                record_id: RecordId::new_v4(),
                winner_txid: "aa".repeat(32),
            },
            proof: ConvergeProofFields {
                convergence_txid: display_hex_to_le32(&"aa".repeat(32)).unwrap(),
                convergence_amount_sat: crate::types::MIN_CONVERGENCE_AMOUNT_SAT,
                bind_hash: [0x55; 32],
                height: 300,
                target_prev: 0x1c00ff00,
                convergence_root: [0u8; 32],
            },
            res_payout_script: witness_v1_res_script(&program),
            oru_change_script: bootstrap_p2wsh_scriptpubkey(),
            funding: OruFundingInput {
                prev_txid: [0x77; 32],
                vout: 0,
                amount: 500_000_000,
                script_pubkey: bootstrap_p2wsh_scriptpubkey(),
            },
            oru_fee: 10_000,
        })
        .unwrap();

        let template = rigor_gbt::build_template_record(
            GbtRecord {
                record_id: RecordId::new_v4(),
                fetched_at: 0,
                tip_hash: TipHash("aa".repeat(32)),
                previousblockhash: "00".repeat(32),
                height: 300,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: "1c00ff00".into(),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 50,
                transactions: vec![
                    GbtTransaction {
                        data: "0100000000010000000001".into(),
                        txid: "aa".repeat(32),
                        hash: None,
                        depends: None,
                        fee: None,
                        sigops: None,
                        weight: None,
                    },
                    GbtTransaction {
                        data: "0100000000020000000001".into(),
                        txid: "bb".repeat(32),
                        hash: None,
                        depends: None,
                        fee: None,
                        sigops: None,
                        weight: None,
                    },
                ],
                default_witness_commitment: None,
                weightlimit: None,
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
                default_coinbase_scriptsig: None,
                default_coinbase_tx: None,
                default_coinbase_txid: None,
                merkleroot: None,
                rigor_coinbase_immutable: None,
                coinbase_address: None,
            },
            None,
            TemplateVersion::INITIAL,
            rigor_gbt::TemplateState::Active,
        );

        let block = assemble_candidate_block(
            &template,
            &sample_job(),
            &artifact.raw_tx_hex,
            &artifact.body_mint_txid,
            &MintBlockContext {
                extranonce1: vec![0x01],
                extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                ntime: 0x65000000,
                nonce: 0x1234,
            },
        )
        .unwrap();

        assert_eq!(block.vtx_count, 4);
        assert!(!block.block_hex.is_empty());
    }
}
