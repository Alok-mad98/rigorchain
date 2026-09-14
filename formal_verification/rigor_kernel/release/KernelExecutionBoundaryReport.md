# Kernel Execution Boundary Report

Level 4.5 formal kernel execution boundary. Coq model only.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

## 1. Current Level

Level 4.5 Formal Kernel Execution Boundary

## 2. Verified

- Coq abstract kernel
- Concrete kernel refinement
- Executable model refinement
- Serialization model

Theorems: `execution_projection_preserves_invariants`,
`executable_step_refines_concrete_step`,
`executable_transition_deterministic`,
`decode_encode_identity` (kernel-state encode/decode).

## 3. Not verified

- C++
- Full node
- Compiler
- Machine code
- Network
- Wallet
- Crypto

See `kernel_binary/BinaryBoundaryTrace.md` and
`release/TCBInventory.md` (layers 0–4).
