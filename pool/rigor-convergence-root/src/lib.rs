//! Canonical convergence root construction — single implementation for the pool stack.
//!
//! Matches `Consensus::ComputeConvergenceRootFromEligibilityTxids` / `convergence_root.cpp`.

mod encoding;
mod root;

pub use encoding::{
    display_hex_to_native, hash_hex_to_native, native_to_display_hex, native_to_hash_hex,
    sethex_to_native, ConvergenceRootError, ConvergenceRootResult, TxidEncoding,
};
pub use root::{
    compute_convergence_root, compute_convergence_root_for_rigord_f,
    compute_convergence_root_from_native, compute_convergence_root_with_encoding, hash256,
};
