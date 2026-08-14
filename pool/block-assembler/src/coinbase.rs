use crate::convergence::compute_convergence_root;
use crate::error::{AssemblerError, AssemblerResult};
use crate::merkle::{
    build_witness_commitment_script, witness_merkle_root_for_block,
};
use crate::script::{
    build_coinbase_scriptsig, build_extranonce_op_return, build_rigor_commitment_script,
    parse_compact_hex,
};
use rigor_gbt::GbtRecord;

fn write_compact_size(buf: &mut Vec<u8>, n: u64) {
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

fn push_bytes(buf: &mut Vec<u8>, data: &[u8]) {
    write_compact_size(buf, data.len() as u64);
    buf.extend_from_slice(data);
}

/// Output of coinbase assembly (pre-merkle).
#[derive(Clone, Debug, PartialEq)]
pub struct CoinbaseAssembly {
    pub coinbase_raw: Vec<u8>,
    pub coinb1_hex: String,
    pub coinb2_hex: String,
    pub extranonce2_size: u32,
    pub scriptsig: Vec<u8>,
    pub convergence_root: [u8; 32],
    pub rigor_script: Vec<u8>,
    pub cvge_script: Option<Vec<u8>>,
    pub witness_commitment_script: Option<Vec<u8>>,
}

pub struct CoinbaseConfig<'a> {
    pub payout_script: &'a [u8],
    /// Must match stratum `extranonce1` length (RIGOR session manager uses 4).
    pub extranonce1_size: u32,
    pub extranonce2_size: u32,
    pub tx_version: u32,
}

pub fn assemble_coinbase(gbt: &GbtRecord, config: &CoinbaseConfig<'_>) -> AssemblerResult<CoinbaseAssembly> {
    if config.payout_script.is_empty() {
        return Err(AssemblerError::Assembly(
            "payout_script must be non-empty".into(),
        ));
    }
    if config.extranonce1_size == 0 || config.extranonce1_size > 8 {
        return Err(AssemblerError::Assembly(format!(
            "extranonce1_size {} out of range 1..=8",
            config.extranonce1_size
        )));
    }
    if config.extranonce2_size == 0 || config.extranonce2_size > 8 {
        return Err(AssemblerError::Assembly(format!(
            "extranonce2_size {} out of range 1..=8",
            config.extranonce2_size
        )));
    }

    let convergence_root = compute_convergence_root(&gbt.convergence_txids);
    let base_scriptsig = build_coinbase_scriptsig(gbt.height, &convergence_root);

    let prev_nbits = parse_compact_hex(&gbt.prev_nbits)
        .map_err(|e| AssemblerError::Assembly(format!("prev_nbits: {e}")))?;
    let rigor_script = build_rigor_commitment_script(prev_nbits);

    let cvge_script = gbt
        .default_convergence_commitment
        .as_ref()
        .and_then(|hex| hex::decode(hex).ok());

    // Presence of GBT default_witness_commitment means segwit is active for this tip.
    // Do NOT copy the node's script bytes — recompute from opaque GBT txs + zero reserved.
    let need_witness = gbt.default_witness_commitment.is_some();
    let en_gap = (config.extranonce1_size + config.extranonce2_size) as usize;

    if need_witness {
        let mut gbt_raws: Vec<Vec<u8>> = Vec::with_capacity(gbt.transactions.len());
        for tx in &gbt.transactions {
            let raw = hex::decode(&tx.data)
                .map_err(|e| AssemblerError::Assembly(format!("gbt tx decode for wtxid: {e}")))?;
            gbt_raws.push(raw);
        }
        let raw_refs: Vec<&[u8]> = gbt_raws.iter().map(|r| r.as_slice()).collect();
        let wroot = witness_merkle_root_for_block(&raw_refs);
        let reserved = [0u8; 32];
        let witness_commitment_script = build_witness_commitment_script(&wroot, &reserved);

        return assemble_segwit_coinbase(
            config,
            &base_scriptsig,
            &rigor_script,
            cvge_script.as_deref(),
            &witness_commitment_script,
            gbt.coinbasevalue as u64,
            en_gap,
            convergence_root,
        );
    }

    assemble_legacy_coinbase(
        config,
        &base_scriptsig,
        &rigor_script,
        cvge_script.as_deref(),
        gbt.coinbasevalue as u64,
        en_gap,
        convergence_root,
    )
}

/// Append value=0 OP_RETURN whose push payload is the extranonce gap.
/// Returns (`split_after_prefix`, `split_after_en`) into `tx` for coinb1/coinb2.
fn push_extranonce_op_return_output(tx: &mut Vec<u8>, en_gap: usize) -> (usize, usize) {
    let en_script = build_extranonce_op_return(en_gap);
    tx.extend_from_slice(&0u64.to_le_bytes());
    push_bytes(tx, &en_script);
    // Payload zeros start after OP_RETURN + push-length byte.
    let split_after_en = tx.len();
    let split_after_prefix = split_after_en - en_gap;
    (split_after_prefix, split_after_en)
}

/// BIP141 wire order: version | marker | flag | vin | vout | witness | locktime.
///
/// Extranonces live in a dedicated **OP_RETURN** output (not scriptSig): RIGOR
/// requires scriptSig = height ‖ ConvergenceRoot with zero trailing bytes.
/// Witness reserved stays 32 zeros so the BIP141 commitment is stable across
/// extranonce search. Commitment script must already be recomputed for this GBT.
fn assemble_segwit_coinbase(
    config: &CoinbaseConfig<'_>,
    base_scriptsig: &[u8],
    rigor_script: &[u8],
    cvge_script: Option<&[u8]>,
    witness_commitment_script: &[u8],
    coinbase_value: u64,
    en_gap: usize,
    convergence_root: [u8; 32],
) -> AssemblerResult<CoinbaseAssembly> {
    let mut tx = Vec::new();
    tx.extend_from_slice(&config.tx_version.to_le_bytes());
    tx.push(0x00); // marker
    tx.push(0x01); // flag

    // vin — consensus-exact scriptSig (no extranonce)
    write_compact_size(&mut tx, 1);
    tx.extend_from_slice(&[0u8; 32]);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());
    push_bytes(&mut tx, base_scriptsig);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());

    // vout: ORU + witness commitment + optional CVGE + RIGOR + extranonce OP_RETURN
    let has_cvge = cvge_script.is_some();
    let out_count = 2 + u64::from(has_cvge) + 1 + 1;
    write_compact_size(&mut tx, out_count);

    tx.extend_from_slice(&coinbase_value.to_le_bytes());
    push_bytes(&mut tx, config.payout_script);

    tx.extend_from_slice(&0u64.to_le_bytes());
    push_bytes(&mut tx, witness_commitment_script);

    if let Some(cvge) = cvge_script {
        tx.extend_from_slice(&0u64.to_le_bytes());
        push_bytes(&mut tx, cvge);
    }

    tx.extend_from_slice(&0u64.to_le_bytes());
    push_bytes(&mut tx, rigor_script);

    let (split_after_prefix, split_after_en) = push_extranonce_op_return_output(&mut tx, en_gap);

    // BIP141: coinbase witness = single 32-byte reserved value (zeros).
    write_compact_size(&mut tx, 1);
    write_compact_size(&mut tx, 32);
    tx.extend_from_slice(&[0u8; 32]);

    tx.extend_from_slice(&0u32.to_le_bytes()); // locktime

    Ok(CoinbaseAssembly {
        coinb1_hex: hex::encode(&tx[..split_after_prefix]),
        coinb2_hex: hex::encode(&tx[split_after_en..]),
        coinbase_raw: tx,
        extranonce2_size: config.extranonce2_size,
        scriptsig: base_scriptsig.to_vec(),
        convergence_root,
        rigor_script: rigor_script.to_vec(),
        cvge_script: cvge_script.map(|s| s.to_vec()),
        witness_commitment_script: Some(witness_commitment_script.to_vec()),
    })
}

/// Non-segwit: extranonces in OP_RETURN (same as segwit — scriptSig stays exact).
fn assemble_legacy_coinbase(
    config: &CoinbaseConfig<'_>,
    base_scriptsig: &[u8],
    rigor_script: &[u8],
    cvge_script: Option<&[u8]>,
    coinbase_value: u64,
    en_gap: usize,
    convergence_root: [u8; 32],
) -> AssemblerResult<CoinbaseAssembly> {
    let mut tx = Vec::new();
    tx.extend_from_slice(&config.tx_version.to_le_bytes());

    write_compact_size(&mut tx, 1);
    tx.extend_from_slice(&[0u8; 32]);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());
    push_bytes(&mut tx, base_scriptsig);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());

    let has_cvge = cvge_script.is_some();
    let out_count = 1 + u64::from(has_cvge) + 1 + 1;
    write_compact_size(&mut tx, out_count);

    tx.extend_from_slice(&coinbase_value.to_le_bytes());
    push_bytes(&mut tx, config.payout_script);

    if let Some(cvge) = cvge_script {
        tx.extend_from_slice(&0u64.to_le_bytes());
        push_bytes(&mut tx, cvge);
    }

    tx.extend_from_slice(&0u64.to_le_bytes());
    push_bytes(&mut tx, rigor_script);

    let (split_after_prefix, split_after_en) = push_extranonce_op_return_output(&mut tx, en_gap);

    tx.extend_from_slice(&0u32.to_le_bytes());

    Ok(CoinbaseAssembly {
        coinb1_hex: hex::encode(&tx[..split_after_prefix]),
        coinb2_hex: hex::encode(&tx[split_after_en..]),
        coinbase_raw: tx,
        extranonce2_size: config.extranonce2_size,
        scriptsig: base_scriptsig.to_vec(),
        convergence_root,
        rigor_script: rigor_script.to_vec(),
        cvge_script: cvge_script.map(|s| s.to_vec()),
        witness_commitment_script: None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::merkle::{build_witness_commitment_script, txid_from_tx_bytes, witness_merkle_root_for_block};
    use crate::script::build_coinbase_scriptsig;
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TipHash};

    fn sample_gbt(with_witness: bool) -> GbtRecord {
        GbtRecord {
            record_id: RecordId::new_v4(),
            fetched_at: 0,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            height: 100,
            longpollid: "lp".into(),
            bits: "1d00ffff".into(),
            prev_nbits: "1c00ff00".into(),
            target: None,
            mintime: Some(0),
            curtime: Some(1_700_000_000),
            coinbasevalue: 5_000_000_000,
            transactions: vec![],
            convergence_txids: vec!["aa".repeat(32), "bb".repeat(32)],
            convergence_root: None,
            convergence_draw: None,
            // Signal only — bytes are intentionally wrong so we prove we recompute.
            default_witness_commitment: with_witness.then(|| hex::encode([0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed])),
            default_convergence_commitment: Some(hex::encode([0x6a, 0x2d])),
            weightlimit: Some(1_000_000),
            version: Some(0x20000000),
            gbt_payload_hash: [0u8; 32],
            raw_json: "{}".into(),
        }
    }

    #[test]
    fn coinbase_contains_height_and_convergence_root() {
        let gbt = sample_gbt(true);
        let mut payout = vec![0x76, 0xa9, 0x14];
        payout.extend(std::iter::repeat_n(0u8, 20));
        payout.push(0x88);
        payout.push(0xac);
        let asm = assemble_coinbase(
            &gbt,
            &CoinbaseConfig {
                payout_script: &payout,
                extranonce1_size: 4,
                extranonce2_size: 4,
                tx_version: 0x20000000,
            },
        )
        .unwrap();
        assert!(asm.coinbase_raw.len() > 60);
        assert_eq!(
            asm.scriptsig,
            build_coinbase_scriptsig(gbt.height, &asm.convergence_root)
        );
        assert_eq!(asm.coinbase_raw[4], 0x00);
        assert_eq!(asm.coinbase_raw[5], 0x01);
        let expected = build_witness_commitment_script(&[0u8; 32], &[0u8; 32]);
        assert_eq!(asm.witness_commitment_script.as_ref().unwrap(), &expected);
    }

    #[test]
    fn extranonce_in_op_return_changes_txid_not_witness_commitment() {
        let gbt = sample_gbt(true);
        let mut payout = vec![0x00, 0x14];
        payout.extend(std::iter::repeat_n(0xa5u8, 20));
        let asm = assemble_coinbase(
            &gbt,
            &CoinbaseConfig {
                payout_script: &payout,
                extranonce1_size: 4,
                extranonce2_size: 4,
                tx_version: 0x20000000,
            },
        )
        .unwrap();
        let txid0 = txid_from_tx_bytes(&asm.coinbase_raw);
        let commit0 = asm.witness_commitment_script.clone().unwrap();

        let coinb1 = hex::decode(&asm.coinb1_hex).unwrap();
        let coinb2 = hex::decode(&asm.coinb2_hex).unwrap();
        let mut solved = coinb1;
        solved.extend_from_slice(&[1, 2, 3, 4]);
        solved.extend_from_slice(&[5, 6, 7, 8]);
        solved.extend_from_slice(&coinb2);
        let txid1 = txid_from_tx_bytes(&solved);
        assert_ne!(txid0, txid1, "OP_RETURN extranonce must change txid");

        // scriptSig stays height‖C only (no trailing en).
        assert_eq!(
            asm.scriptsig,
            build_coinbase_scriptsig(gbt.height, &asm.convergence_root)
        );

        // Witness reserved still zeros ⇒ commitment bytes still present & unchanged.
        assert!(solved.windows(commit0.len()).any(|w| w == commit0.as_slice()));
        assert_eq!(
            witness_merkle_root_for_block(&[]),
            [0u8; 32]
        );
    }

    #[test]
    fn gbt_body_tx_enters_witness_commitment() {
        // Minimal non-segwit fake tx bytes as a body leaf.
        let body = hex::encode({
            let mut t = Vec::new();
            t.extend_from_slice(&1u32.to_le_bytes());
            t.push(0x00); // vin=0
            t.push(0x00); // vout=0
            t.extend_from_slice(&0u32.to_le_bytes());
            t
        });
        let mut gbt = sample_gbt(true);
        gbt.transactions.push(GbtTransaction {
            data: body.clone(),
            txid: "11".repeat(32),
            hash: None,
            depends: None,
            fee: None,
            sigops: None,
            weight: None,
        });
        let mut payout = vec![0x00, 0x14];
        payout.extend(std::iter::repeat_n(0x22u8, 20));
        let asm = assemble_coinbase(
            &gbt,
            &CoinbaseConfig {
                payout_script: &payout,
                extranonce1_size: 4,
                extranonce2_size: 4,
                tx_version: 0x20000000,
            },
        )
        .unwrap();
        let raw = hex::decode(&body).unwrap();
        let wroot = witness_merkle_root_for_block(&[raw.as_slice()]);
        let expect = build_witness_commitment_script(&wroot, &[0u8; 32]);
        assert_eq!(asm.witness_commitment_script.as_ref().unwrap(), &expect);
        assert_ne!(expect, build_witness_commitment_script(&[0u8; 32], &[0u8; 32]));
    }
}
