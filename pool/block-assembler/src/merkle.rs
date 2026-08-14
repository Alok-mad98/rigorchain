use crate::convergence::hash256;

/// Build merkle branches for coinbase at index 0.
///
/// Branch hex is **internal byte order** (same as Bitcoin Core merkle pairing).
pub fn build_merkle_branches(tx_hashes: &[[u8; 32]]) -> ([u8; 32], Vec<String>) {
    if tx_hashes.is_empty() {
        return ([0u8; 32], Vec::new());
    }

    let mut level: Vec<[u8; 32]> = tx_hashes.to_vec();
    let mut branches = Vec::new();

    while level.len() > 1 {
        branches.push(hex::encode(level[1]));
        let mut next = Vec::new();
        let mut i = 0;
        while i < level.len() {
            if i + 1 < level.len() {
                let mut pair = Vec::with_capacity(64);
                pair.extend_from_slice(&level[i]);
                pair.extend_from_slice(&level[i + 1]);
                next.push(hash256(&pair));
                i += 2;
            } else {
                let mut pair = Vec::with_capacity(64);
                pair.extend_from_slice(&level[i]);
                pair.extend_from_slice(&level[i]);
                next.push(hash256(&pair));
                i += 1;
            }
        }
        level = next;
    }

    (level[0], branches)
}

/// Compute the Bitcoin block-merkle leaf for a serialized transaction: **txid**
/// (witness stripped). Never use wtxid here.
pub fn txid_from_tx_bytes(tx_bytes: &[u8]) -> [u8; 32] {
    if let Some(txid) = segwit_txid_without_witness(tx_bytes) {
        return txid;
    }
    hash256(tx_bytes)
}

/// Witness txid: Hash256 of the full wire serialization (includes witness).
/// For non-segwit txs this equals the txid.
pub fn wtxid_from_tx_bytes(tx_bytes: &[u8]) -> [u8; 32] {
    hash256(tx_bytes)
}

/// Legacy alias — returns txid, not a raw blob hash of witness txs.
pub fn tx_hash_from_raw(tx_bytes: &[u8]) -> [u8; 32] {
    txid_from_tx_bytes(tx_bytes)
}

/// BIP141: coinbase contributes the null wtxid; remaining leaves are GBT wtxids.
pub fn witness_merkle_root_for_block(gbt_tx_raws: &[&[u8]]) -> [u8; 32] {
    let mut leaves: Vec<[u8; 32]> = Vec::with_capacity(1 + gbt_tx_raws.len());
    leaves.push([0u8; 32]); // coinbase wtxid is defined as null
    for raw in gbt_tx_raws {
        leaves.push(wtxid_from_tx_bytes(raw));
    }
    build_merkle_branches(&leaves).0
}

/// `OP_RETURN OP_PUSH36 aa21a9ed || SHA256D(witness_root || reserved)`.
pub fn build_witness_commitment_script(
    witness_root: &[u8; 32],
    reserved: &[u8; 32],
) -> Vec<u8> {
    let mut preimage = Vec::with_capacity(64);
    preimage.extend_from_slice(witness_root);
    preimage.extend_from_slice(reserved);
    let commit = hash256(&preimage);
    let mut script = Vec::with_capacity(38);
    script.extend_from_slice(&[0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed]);
    script.extend_from_slice(&commit);
    script
}

/// Decode a 32-byte hash hex from Bitcoin Core RPC **display** order into native
/// (internal) byte order used in merkle trees / block headers.
pub fn display_hash_hex_to_native(hex_str: &str) -> Result<[u8; 32], String> {
    let mut bytes =
        hex::decode(hex_str.trim()).map_err(|e| format!("hash hex decode: {e}"))?;
    if bytes.len() != 32 {
        return Err(format!("hash must be 32 bytes, got {}", bytes.len()));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

fn segwit_txid_without_witness(tx_raw: &[u8]) -> Option<[u8; 32]> {
    if tx_raw.len() < 10 || tx_raw[4] != 0x00 || tx_raw[5] != 0x01 {
        return None;
    }
    let mut off = 6usize;
    let vin_count = read_varint(tx_raw, &mut off)? as usize;
    for _ in 0..vin_count {
        if off + 36 > tx_raw.len() {
            return None;
        }
        off += 36;
        let script_len = read_varint(tx_raw, &mut off)? as usize;
        if off + script_len + 4 > tx_raw.len() {
            return None;
        }
        off += script_len;
        off += 4;
    }
    let vout_count = read_varint(tx_raw, &mut off)? as usize;
    for _ in 0..vout_count {
        if off + 8 > tx_raw.len() {
            return None;
        }
        off += 8;
        let script_len = read_varint(tx_raw, &mut off)? as usize;
        if off + script_len > tx_raw.len() {
            return None;
        }
        off += script_len;
    }
    let vin_vout_end = off;
    for _ in 0..vin_count {
        let n_items = read_varint(tx_raw, &mut off)? as usize;
        for _ in 0..n_items {
            let item_len = read_varint(tx_raw, &mut off)? as usize;
            if off + item_len > tx_raw.len() {
                return None;
            }
            off += item_len;
        }
    }
    if off + 4 != tx_raw.len() {
        return None;
    }
    let mut base = Vec::with_capacity(4 + (vin_vout_end - 6) + 4);
    base.extend_from_slice(&tx_raw[0..4]);
    base.extend_from_slice(&tx_raw[6..vin_vout_end]);
    base.extend_from_slice(&tx_raw[tx_raw.len() - 4..]);
    Some(hash256(&base))
}

fn read_varint(buf: &[u8], off: &mut usize) -> Option<u64> {
    if *off >= buf.len() {
        return None;
    }
    let first = buf[*off];
    *off += 1;
    match first {
        0..=0xfc => Some(first as u64),
        0xfd => {
            if *off + 2 > buf.len() {
                return None;
            }
            let v = u16::from_le_bytes(buf[*off..*off + 2].try_into().ok()?) as u64;
            *off += 2;
            Some(v)
        }
        0xfe => {
            if *off + 4 > buf.len() {
                return None;
            }
            let v = u32::from_le_bytes(buf[*off..*off + 4].try_into().ok()?) as u64;
            *off += 4;
            Some(v)
        }
        _ => {
            if *off + 8 > buf.len() {
                return None;
            }
            let v = u64::from_le_bytes(buf[*off..*off + 8].try_into().ok()?);
            *off += 8;
            Some(v)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_tx_merkle_is_tx_hash() {
        let h = [0xab; 32];
        let (root, branches) = build_merkle_branches(&[h]);
        assert_eq!(root, h);
        assert!(branches.is_empty());
    }

    #[test]
    fn coinbase_only_witness_commitment_matches_core() {
        let root = witness_merkle_root_for_block(&[]);
        assert_eq!(root, [0u8; 32]);
        let reserved = [0u8; 32];
        let script = build_witness_commitment_script(&root, &reserved);
        assert_eq!(&script[..6], &[0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed]);
        let pre = [0u8; 64];
        let expect = hash256(&pre);
        assert_eq!(&script[6..], &expect);
        let other = build_witness_commitment_script(&root, &[1u8; 32]);
        assert_ne!(script, other);
    }

    #[test]
    fn segwit_txid_strips_marker_flag_witness() {
        let mut tx = Vec::new();
        tx.extend_from_slice(&1u32.to_le_bytes());
        tx.push(0x00);
        tx.push(0x01);
        tx.push(0x01);
        tx.extend_from_slice(&[0u8; 32]);
        tx.extend_from_slice(&0xffffffffu32.to_le_bytes());
        tx.push(0x00);
        tx.extend_from_slice(&0xffffffffu32.to_le_bytes());
        tx.push(0x01);
        tx.extend_from_slice(&0u64.to_le_bytes());
        tx.push(0x00);
        tx.push(0x01);
        tx.push(0x20);
        tx.extend_from_slice(&[0u8; 32]);
        tx.extend_from_slice(&0u32.to_le_bytes());

        let txid = txid_from_tx_bytes(&tx);
        let mut base = Vec::new();
        base.extend_from_slice(&1u32.to_le_bytes());
        base.push(0x01);
        base.extend_from_slice(&[0u8; 32]);
        base.extend_from_slice(&0xffffffffu32.to_le_bytes());
        base.push(0x00);
        base.extend_from_slice(&0xffffffffu32.to_le_bytes());
        base.push(0x01);
        base.extend_from_slice(&0u64.to_le_bytes());
        base.push(0x00);
        base.extend_from_slice(&0u32.to_le_bytes());
        assert_eq!(txid, hash256(&base));
        assert_eq!(wtxid_from_tx_bytes(&tx), hash256(&tx));
        assert_ne!(txid, wtxid_from_tx_bytes(&tx));
    }

    #[test]
    fn display_hex_reverses_to_native() {
        let native = display_hash_hex_to_native(
            "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20",
        )
        .unwrap();
        assert_eq!(native[0], 0x20);
        assert_eq!(native[31], 0x01);
    }
}
