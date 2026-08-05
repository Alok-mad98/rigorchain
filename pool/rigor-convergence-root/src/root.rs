use sha2::{Digest, Sha256};

use crate::encoding::{display_hex_to_native, hash_hex_to_native, sethex_to_native, TxidEncoding};

/// Hash256 = SHA256(SHA256(x)) per RIGOR consensus.
pub fn hash256(data: &[u8]) -> [u8; 32] {
    let first = Sha256::digest(data);
    let second = Sha256::digest(&first);
    let mut out = [0u8; 32];
    out.copy_from_slice(&second);
    out
}

/// Compute **C** from GBT display-order txids (canonical pool publish path).
pub fn compute_convergence_root(txid_display_hex: &[String]) -> [u8; 32] {
    compute_convergence_root_with_encoding(txid_display_hex, TxidEncoding::Display)
}

/// Compute **C** when **F** cardinality is already rigord-authoritative (live replay / GCR).
///
/// When `rigord_f_size < 2`, returns zero root regardless of `txid_display_hex` length
/// (matches `ComputeConvergenceRootFromEligibilityTxids` boundary).
pub fn compute_convergence_root_for_rigord_f(
    rigord_f_size: usize,
    txid_display_hex: &[String],
) -> [u8; 32] {
    if rigord_f_size < 2 {
        return [0u8; 32];
    }
    compute_convergence_root(txid_display_hex)
}

/// Compute **C** from uint256 native txid bytes (draw replay internal path).
pub fn compute_convergence_root_from_native(txids_native: &[[u8; 32]]) -> [u8; 32] {
    if txids_native.len() < 2 {
        return [0u8; 32];
    }
    let mut sorted = txids_native.to_vec();
    sorted.sort();
    let mut concat = Vec::with_capacity(sorted.len() * 32);
    for id in &sorted {
        concat.extend_from_slice(id);
    }
    hash256(&concat)
}

pub fn compute_convergence_root_with_encoding(
    txid_hex: &[String],
    encoding: TxidEncoding,
) -> [u8; 32] {
    if txid_hex.len() < 2 {
        return [0u8; 32];
    }

    let mut txids: Vec<[u8; 32]> = Vec::with_capacity(txid_hex.len());
    for hex_str in txid_hex {
        let parsed = match encoding {
            TxidEncoding::Display => display_hex_to_native(hex_str),
            TxidEncoding::HashHex => hash_hex_to_native(hex_str),
            TxidEncoding::SetHex => sethex_to_native(hex_str),
        };
        if let Ok(native) = parsed {
            txids.push(native);
        }
    }

    compute_convergence_root_from_native(&txids)
}
