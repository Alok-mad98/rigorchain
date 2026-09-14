use crate::types::BodyMintPlaceholder;

/// Historical helper: previously hashed `record_seed` into a fake merkle leaf.
/// Retained for schema/audit callers; ORU-only assembly never inserts this leaf.
pub fn body_mint_placeholder_txid(record_seed: &[u8]) -> String {
    use sha2::{Digest, Sha256};
    let digest = Sha256::digest(record_seed);
    hex::encode(digest)
}

/// Body-mint slot metadata for JobSnapshot schema compatibility.
///
/// RFC-020: `reserved=false` and `mint_required=false` always. Callers that pass
/// `mint_required=true` are refused by [`crate::assembly::assemble_from_gbt`].
pub fn build_body_mint_placeholder(
    gbt_tx_count: usize,
    mint_required: bool,
    _record_seed: &[u8],
) -> BodyMintPlaceholder {
    let _ = mint_required;
    BodyMintPlaceholder {
        // Informational: first index after coinbase + GBT package (unused for merkle).
        slot_index: gbt_tx_count + 1,
        reserved: false,
        mint_required: false,
        body_mint_txid: None,
        placeholder_txid: String::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn placeholder_txid_helper_is_stable() {
        let a = body_mint_placeholder_txid(b"seed");
        let b = body_mint_placeholder_txid(b"seed");
        assert_eq!(a, b);
        assert_eq!(a.len(), 64);
    }

    #[test]
    fn oru_only_never_reserves_mint_slot() {
        let p = build_body_mint_placeholder(3, true, b"x");
        assert_eq!(p.slot_index, 4);
        assert!(!p.reserved);
        assert!(!p.mint_required);
        assert!(p.body_mint_txid.is_none());
        assert!(p.placeholder_txid.is_empty());
    }
}
