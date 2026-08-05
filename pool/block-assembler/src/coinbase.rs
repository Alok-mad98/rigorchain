use crate::convergence::compute_convergence_root;
use crate::error::{AssemblerError, AssemblerResult};
use crate::script::{build_coinbase_scriptsig, build_rigor_commitment_script, parse_compact_hex};
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
    pub extranonce2_size: u32,
    pub tx_version: u32,
}

pub fn assemble_coinbase(gbt: &GbtRecord, config: &CoinbaseConfig<'_>) -> AssemblerResult<CoinbaseAssembly> {
    if config.payout_script.is_empty() {
        return Err(AssemblerError::Assembly(
            "payout_script must be non-empty".into(),
        ));
    }

    let convergence_root = compute_convergence_root(&gbt.convergence_txids);
    let scriptsig = build_coinbase_scriptsig(gbt.height, &convergence_root);

    let prev_nbits = parse_compact_hex(&gbt.prev_nbits)
        .map_err(|e| AssemblerError::Assembly(format!("prev_nbits: {e}")))?;
    let rigor_script = build_rigor_commitment_script(prev_nbits);

    let cvge_script = gbt
        .default_convergence_commitment
        .as_ref()
        .and_then(|hex| hex::decode(hex).ok());

    let witness_commitment_script = gbt
        .default_witness_commitment
        .as_ref()
        .and_then(|hex| hex::decode(hex).ok());

    let mut tx = Vec::new();
    tx.extend_from_slice(&config.tx_version.to_le_bytes());

    // vin
    write_compact_size(&mut tx, 1);
    tx.extend_from_slice(&[0u8; 32]);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());
    push_bytes(&mut tx, &scriptsig);
    tx.extend_from_slice(&0xffffffffu32.to_le_bytes());

    // vout count: ORU + optional witness + optional CVGE + RIGOR
    let has_witness = witness_commitment_script.is_some();
    let has_cvge = cvge_script.is_some();
    let out_count = 1 + u64::from(has_witness) + u64::from(has_cvge) + 1;
    write_compact_size(&mut tx, out_count);

    // vout[0] ORU payout
    tx.extend_from_slice(&(gbt.coinbasevalue as u64).to_le_bytes());
    push_bytes(&mut tx, config.payout_script);

    // witness commitment output (zero value)
    if let Some(ref wc_script) = witness_commitment_script {
        tx.extend_from_slice(&0u64.to_le_bytes());
        push_bytes(&mut tx, wc_script);
    }

    // CVGE output
    if let Some(ref cvge) = cvge_script {
        tx.extend_from_slice(&0u64.to_le_bytes());
        push_bytes(&mut tx, cvge);
    }

    // RIGOR commitment (always)
    tx.extend_from_slice(&0u64.to_le_bytes());
    push_bytes(&mut tx, &rigor_script);

    tx.extend_from_slice(&0u32.to_le_bytes());

    // Segwit extension: marker + flag + witness for vin[0]
    let split_before_witness = tx.len();
    if has_witness {
        tx.push(0x00);
        tx.push(0x01);
        write_compact_size(&mut tx, 1);
        write_compact_size(&mut tx, 2);
        write_compact_size(&mut tx, 0);
        write_compact_size(&mut tx, 32);
        let reserved_prefix = 32usize.saturating_sub(config.extranonce2_size as usize);
        tx.extend(std::iter::repeat_n(0u8, reserved_prefix));
        let split_after_reserved_prefix = tx.len();
        tx.extend(std::iter::repeat_n(0u8, config.extranonce2_size as usize));
        let split_after_extranonce_gap = tx.len();
        write_compact_size(&mut tx, 0);

        let coinb1_hex = hex::encode(&tx[..split_after_reserved_prefix]);
        let coinb2_hex = hex::encode(&tx[split_after_extranonce_gap..]);
        return Ok(CoinbaseAssembly {
            coinbase_raw: tx,
            coinb1_hex,
            coinb2_hex,
            extranonce2_size: config.extranonce2_size,
            scriptsig,
            convergence_root,
            rigor_script,
            cvge_script,
            witness_commitment_script,
        });
    }

    // Non-segwit fallback: split at midpoint of trailing metadata for extranonce gap
    let _ = split_before_witness;
    let gap = config.extranonce2_size as usize;
    if tx.len() <= gap {
        return Err(AssemblerError::Assembly("coinbase too small for extranonce gap".into()));
    }
    let split_at = tx.len() - gap;
    Ok(CoinbaseAssembly {
        coinb1_hex: hex::encode(&tx[..split_at]),
        coinb2_hex: hex::encode(&tx[split_at..]),
        coinbase_raw: tx,
        extranonce2_size: config.extranonce2_size,
        scriptsig,
        convergence_root,
        rigor_script,
        cvge_script,
        witness_commitment_script,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TipHash};

    fn sample_gbt() -> GbtRecord {
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
            transactions: vec![GbtTransaction {
                data: "0100000001".into(),
                txid: "bb".repeat(32),
                hash: None,
                depends: None,
                fee: None,
                sigops: None,
                weight: None,
            }],
            convergence_txids: vec!["aa".repeat(32), "bb".repeat(32)],
            convergence_root: None,
            convergence_draw: None,
            default_witness_commitment: Some(hex::encode([
                0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed,
            ])),
            default_convergence_commitment: Some(hex::encode([0x6a, 0x2d])),
            weightlimit: Some(4_000_000),
            version: Some(0x20000000),
            gbt_payload_hash: [0u8; 32],
            raw_json: "{}".into(),
        }
    }

    #[test]
    fn coinbase_contains_height_and_convergence_root() {
        let gbt = sample_gbt();
        let mut payout = vec![0x76, 0xa9, 0x14];
        payout.extend(std::iter::repeat_n(0u8, 20));
        payout.push(0x88);
        payout.push(0xac);
        let asm = assemble_coinbase(
            &gbt,
            &CoinbaseConfig {
                payout_script: &payout,
                extranonce2_size: 4,
                tx_version: 0x20000000,
            },
        )
        .unwrap();
        assert!(asm.coinbase_raw.len() > 60);
        assert!(asm.scriptsig.len() >= 34); // height push + OP_PUSH32 + C
        assert_eq!(asm.convergence_root.len(), 32);
        assert!(asm.rigor_script.starts_with(&[0x6a, 0x08]));
        assert!(!asm.coinb1_hex.is_empty());
        assert!(!asm.coinb2_hex.is_empty());
    }
}
