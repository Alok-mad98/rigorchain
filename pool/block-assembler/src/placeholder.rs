use crate::types::BodyMintPlaceholder;
use sha2::{Digest, Sha256};

/// Fixed placeholder txid for reserved body-mint vtx slot (immutable until Mint Engine fills).
pub fn body_mint_placeholder_txid(record_seed: &[u8]) -> String {
    let digest = Sha256::digest(record_seed);
    hex::encode(digest)
}

pub fn build_body_mint_placeholder(
    gbt_tx_count: usize,
    mint_required: bool,
    record_seed: &[u8],
) -> BodyMintPlaceholder {
    BodyMintPlaceholder {
        slot_index: gbt_tx_count + 1,
        reserved: true,
        mint_required,
        body_mint_txid: None,
        placeholder_txid: body_mint_placeholder_txid(record_seed),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn placeholder_txid_is_stable() {
        let a = body_mint_placeholder_txid(b"seed");
        let b = body_mint_placeholder_txid(b"seed");
        assert_eq!(a, b);
        assert_eq!(a.len(), 64);
    }

    #[test]
    fn slot_index_follows_gbt_package() {
        let p = build_body_mint_placeholder(3, false, b"x");
        assert_eq!(p.slot_index, 4);
        assert!(p.reserved);
        assert!(!p.mint_required);
        assert!(p.body_mint_txid.is_none());
    }
}
