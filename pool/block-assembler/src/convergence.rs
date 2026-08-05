//! Re-export canonical convergence root from `rigor-convergence-root` (TASK-MINING-020).

pub use rigor_convergence_root::{
    compute_convergence_root, compute_convergence_root_for_rigord_f,
    compute_convergence_root_from_native, hash256, native_to_hash_hex,
};
