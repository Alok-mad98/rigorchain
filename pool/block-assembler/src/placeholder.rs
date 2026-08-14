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
/// TASK-POOL-RES-BOUNDARY-IMPLEMENT-003 (Option A): the pool **never** reserves a
/// merkle placeholder for RES mint and **never** marks `mint_required`. RES body
/// mint bytes, when required by consensus, arrive only as opaque GBT transactions
/// from `rigord` automint — the pool does not invent them.
pub fn build_body_mint_placeholder(
    gbt_tx_count: usize,
    _mint_required: bool,
    _record_seed: &[u8],
) -> BodyMintPlaceholder {
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
