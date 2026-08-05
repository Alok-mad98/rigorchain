use crate::error::{MinerError, MinerResult};
use crate::stratum::messages::MiningNotify;
use share_validator::opuhash::hash256;

/// Build 80-byte block header from Stratum notify + miner deltas (matches share-validator).
pub fn build_header_bytes(
    notify: &MiningNotify,
    extranonce1: &[u8],
    extranonce2: &[u8],
    ntime: u32,
    nonce: u32,
) -> MinerResult<[u8; 80]> {
    let coinbase_raw = assemble_coinbase_raw(notify, extranonce1, extranonce2)?;
    let coinbase_hash = hash256(&coinbase_raw);
    let merkle_root = compute_merkle_root(&coinbase_hash, &notify.merkle_branch)?;

    let version = u32::from_str_radix(&notify.version, 16)
        .map_err(|e| MinerError::Job(format!("invalid version: {e}")))?;
    let nbits = parse_nbits(&notify.nbits)?;
    let prevhash = decode_prevhash(&notify.prevhash)?;

    let mut header = [0u8; 80];
    header[0..4].copy_from_slice(&version.to_le_bytes());
    header[4..36].copy_from_slice(&prevhash);
    header[36..68].copy_from_slice(&merkle_root);
    header[68..72].copy_from_slice(&ntime.to_le_bytes());
    header[72..76].copy_from_slice(&nbits);
    header[76..80].copy_from_slice(&nonce.to_le_bytes());
    Ok(header)
}

pub fn header_base_hash(header: &[u8; 80]) -> [u8; 32] {
    hash256(header)
}

fn assemble_coinbase_raw(
    notify: &MiningNotify,
    extranonce1: &[u8],
    extranonce2: &[u8],
) -> MinerResult<Vec<u8>> {
    let coinb1 = hex::decode(&notify.coinb1)
        .map_err(|e| MinerError::Job(format!("coinb1 hex: {e}")))?;
    let coinb2 = hex::decode(&notify.coinb2)
        .map_err(|e| MinerError::Job(format!("coinb2 hex: {e}")))?;

    let en2_size = extranonce2.len();
    if en2_size == 0 {
        return Err(MinerError::Job("extranonce2 empty".into()));
    }

    let mut raw = Vec::with_capacity(coinb1.len() + extranonce1.len() + en2_size + coinb2.len());
    raw.extend_from_slice(&coinb1);
    raw.extend_from_slice(extranonce1);
    raw.extend_from_slice(extranonce2);
    raw.extend_from_slice(&coinb2);
    Ok(raw)
}

fn compute_merkle_root(coinbase_hash: &[u8; 32], branches: &[String]) -> MinerResult<[u8; 32]> {
    let mut hash = *coinbase_hash;
    let mut index = 0usize;
    for branch_hex in branches {
        let branch_bytes = hex::decode(branch_hex)
            .map_err(|e| MinerError::Job(format!("merkle branch: {e}")))?;
        if branch_bytes.len() != 32 {
            return Err(MinerError::Job("merkle branch must be 32 bytes".into()));
        }
        let mut branch = [0u8; 32];
        branch.copy_from_slice(&branch_bytes);

        let mut pair = Vec::with_capacity(64);
        if index % 2 == 0 {
            pair.extend_from_slice(&hash);
            pair.extend_from_slice(&branch);
        } else {
            pair.extend_from_slice(&branch);
            pair.extend_from_slice(&hash);
        }
        hash = hash256(&pair);
        index += 1;
    }
    Ok(hash)
}

fn decode_prevhash(hex_str: &str) -> MinerResult<[u8; 32]> {
    let bytes = hex::decode(hex_str).map_err(|e| MinerError::Job(format!("prevhash: {e}")))?;
    if bytes.len() != 32 {
        return Err(MinerError::Job("prevhash must be 32 bytes".into()));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    out.reverse();
    Ok(out)
}

fn parse_nbits(bits_hex: &str) -> MinerResult<[u8; 4]> {
    let compact = u32::from_str_radix(bits_hex, 16)
        .map_err(|e| MinerError::Job(format!("nbits: {e}")))?;
    Ok(compact.to_le_bytes())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_notify() -> MiningNotify {
        MiningNotify {
            job_id: "j1".into(),
            prevhash: "00".repeat(32),
            coinb1: "01".into(),
            coinb2: "02".into(),
            merkle_branch: vec![],
            version: "20000000".into(),
            nbits: "207fffff".into(),
            ntime: "65000000".into(),
            clean_jobs: false,
        }
    }

    #[test]
    fn builds_eighty_byte_header() {
        let h = build_header_bytes(&sample_notify(), &[0xab], &[0x01, 0, 0, 0], 0x6500_0000, 1)
            .unwrap();
        assert_eq!(h.len(), 80);
        assert_eq!(&h[76..80], &1u32.to_le_bytes());
    }
}
