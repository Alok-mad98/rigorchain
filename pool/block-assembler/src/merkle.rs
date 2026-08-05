use crate::convergence::hash256;

/// Build merkle branches for coinbase at index 0 (Stratum-style reversed hex).
pub fn build_merkle_branches(tx_hashes: &[[u8; 32]]) -> ( [u8; 32], Vec<String>) {
    if tx_hashes.is_empty() {
        return ([0u8; 32], Vec::new());
    }

    let mut level: Vec<[u8; 32]> = tx_hashes.to_vec();
    let mut branches = Vec::new();

    while level.len() > 1 {
        branches.push(hex::encode(reverse_bytes(level[1])));
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

fn reverse_bytes(mut hash: [u8; 32]) -> [u8; 32] {
    hash.reverse();
    hash
}

/// Compute txid Hash256 from raw transaction bytes (internal byte order).
pub fn tx_hash_from_raw(tx_bytes: &[u8]) -> [u8; 32] {
    hash256(tx_bytes)
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
}
