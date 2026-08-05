//! Re-export canonical root from `rigor-convergence-root` (TASK-MINING-020).

pub use rigor_convergence_root::{
    compute_convergence_root, compute_convergence_root_for_rigord_f,
    compute_convergence_root_from_native, compute_convergence_root_with_encoding,
    native_to_hash_hex, TxidEncoding,
};
