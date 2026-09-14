//! Production PoW: SHA3-256("RGRPOW01" || header_80).
//!
//! Byte-identical to `rigor_kernel::GetRigorPowHash` / miner `rgrpow_hash80`.
//! Merkle / txid still use SHA256d (`opuhash::hash256`). This module is PoW only.

use sha3::{Digest, Sha3_256};

use crate::opuhash::uint256_le_cmp;

pub const RGRPOW_DOMAIN_TAG: &[u8; 8] = b"RGRPOW01";
pub const RGRPOW_HEADER_LEN: usize = 80;

/// SHA3-256("RGRPOW01" || header_80). Digest byte 0 is the LSB limb vs target.
pub fn rgrpow_hash80(header: &[u8; RGRPOW_HEADER_LEN]) -> [u8; 32] {
    let mut hasher = Sha3_256::new();
    hasher.update(RGRPOW_DOMAIN_TAG);
    hasher.update(header);
    hasher.finalize().into()
}

pub fn verify_rgrpow_share(header: &[u8; 80], share_target: &[u8; 32]) -> ([u8; 32], bool) {
    let pow = rgrpow_hash80(header);
    (pow, uint256_le_cmp(&pow, share_target) != std::cmp::Ordering::Greater)
}

pub fn verify_rgrpow_block(header: &[u8; 80], block_target: &[u8; 32]) -> ([u8; 32], bool) {
    verify_rgrpow_share(header, block_target)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::opuhash::{expand_compact_bits, get_opus_hash_from_base_hash, hash256};

    #[test]
    fn zero_header_matches_node_get_rigor_pow_hash() {
        let want = hex::decode("dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc")
            .unwrap();
        let got = rgrpow_hash80(&[0u8; 80]);
        assert_eq!(got.as_slice(), want.as_slice());
        assert_eq!(got[0], 0xdd);
    }

    #[test]
    fn opuhash_of_zero_header_is_not_sha3_rgrpow() {
        let header = [0u8; 80];
        let sha3 = rgrpow_hash80(&header);
        let opus = get_opus_hash_from_base_hash(&hash256(&header));
        assert_ne!(sha3, opus);
        let tight = expand_compact_bits("03000001").unwrap();
        assert!(!verify_rgrpow_share(&header, &tight).1);
    }

    #[test]
    fn miner_can_meet_regtest_nbits() {
        let target = expand_compact_bits("207fffff").unwrap();
        let mut header = [0u8; 80];
        header[0] = 1;
        let mut found = false;
        for n in 0u32..400_000 {
            header[76..80].copy_from_slice(&n.to_le_bytes());
            if verify_rgrpow_share(&header, &target).1 {
                found = true;
                break;
            }
        }
        assert!(found, "no SHA3 nonce met 0x207fffff");
    }
}
