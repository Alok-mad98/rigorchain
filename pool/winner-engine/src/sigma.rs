use crate::hash::hash256;

/// σ_i = Hash256(txid_i ‖ C ‖ B_{h-1} ‖ LE32(height)) over exactly 100 bytes.
///
/// Port of `rigor::ComputeSigmaHash`. All three 32-byte arguments are raw uint256 bytes in
/// native order, never RPC display order.
///
/// `prev_block_hash` is the parent block hash, inserted verbatim at offset 64 — no inner hash
/// and no ancestor at any other depth. Together with `txid` and `convergence_root`, which the
/// parent's merkle root commits, that makes every σ input a function of one already-mined
/// header: re-rolling the draw costs a proof-of-work solution rather than a hash. The parent's
/// compact target is not a separate input because the parent header already commits its nBits.
pub fn compute_sigma_hash(
    txid_native: &[u8; 32],
    convergence_root_native: &[u8; 32],
    prev_block_hash_native: &[u8; 32],
    height: u32,
) -> [u8; 32] {
    let mut buf = [0u8; 100];
    buf[0..32].copy_from_slice(txid_native);
    buf[32..64].copy_from_slice(convergence_root_native);
    buf[64..96].copy_from_slice(prev_block_hash_native);
    buf[96..100].copy_from_slice(&height.to_le_bytes());
    hash256(&buf)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The parent hash enters the preimage unfolded, so σ must differ from the value produced
    /// by hashing it first — the pre-A8 `bind_commit` construction.
    #[test]
    fn parent_hash_is_not_folded_through_an_inner_hash() {
        let txid = [0x01u8; 32];
        let c = [0x02u8; 32];
        let parent = [0x03u8; 32];

        let mut folded_preimage = Vec::with_capacity(36);
        folded_preimage.extend_from_slice(&parent);
        folded_preimage.extend_from_slice(&0x1d00ffffu32.to_le_bytes());
        let folded = hash256(&folded_preimage);

        assert_ne!(
            compute_sigma_hash(&txid, &c, &parent, 100),
            compute_sigma_hash(&txid, &c, &folded, 100)
        );
    }

    #[test]
    fn seed_change_reshuffles_sigma() {
        let txid = [0xaau8; 32];
        let c = [0xbbu8; 32];
        let mut parent = [0x00u8; 32];
        let a = compute_sigma_hash(&txid, &c, &parent, 7);
        parent[31] = 0x01;
        assert_ne!(a, compute_sigma_hash(&txid, &c, &parent, 7));
    }
}
