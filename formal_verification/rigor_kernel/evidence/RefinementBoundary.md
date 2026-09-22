# Refinement Boundary

Formal specification → Coq kernel → Coq validators.
This document freezes **where the proof stops**.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

This is a *boundary* marker. It is not a CompCert refinement, not an
extraction refinement, not a binary semantic proof, not a C++
equivalence, and not a full-node correctness claim.

## Verified layers

### Layer 0 — Consensus Specification

P0-106 architecture IDs for the critical consensus kernel
(UTXO, ORU, RES, Script, PoW, ASERT). Specification text only.

### Layer 1 — Coq Kernel Model

`formal_verification/rigor_kernel/` models and kernel theorems
(`utxo_soundness`, `oru_conservation_extended`, `no_hidden_inflation`,
`res_issuance_soundness`, `recognizer_pairwise_disjoint`,
`pow_valid_rejects_above_target`, `pow_asert_correctness`,
`negative_time_error_preserved`).

### Layer 2 — Coq Kernel Validators

Boolean `check_*` functions in `Kernel/*Validator.v` and
`kernel_sound` in `Kernel/KernelSoundness.v`. Same rules as Layer 1.

## Unverified layers (outside proof boundary)

### Layer 3 — OCaml extraction

`extraction/ExtractedKernel.ml` is mechanically generated. Extraction
is **outside the proof boundary**.

### Layer 4 — `rigor-kernel-checker` binary

OCaml compilation of Layer 3 plus `kernel_binary/main.ml` (JSON
driver). This is a generated executable artifact for testing, not a
proved executable implementation.

## Proof coverage

```
Spec
 |
 v
Coq
 |
 v
Validator
```

Covered by Coq proofs and `Print Assumptions` Closed:

- Layer 0 → Layer 1 (kernel theorems)
- Layer 1 → Layer 2 (`kernel_sound`)

**Not covered:**

- Layer 2 → Layer 3 (extraction)
- Layer 3 → Layer 4 (compiler / binary)

Machine-checked documentation marker: `Kernel/RefinementBoundary.v`,
`kernel_boundary_closed` (class C; proves a documentation `True`).
See `TheoremClassification.md`.

## Stop position

```
Coq validators
        |
        X
outside proof boundary
        |
OCaml extraction
        |
compiler / binary / runtime
```

Outside the proof boundary (trusted, not proved):

- Coq extraction mechanism (`ExtrOcamlZInt` / `ExtrOcamlNatInt`)
- OCaml compiler correctness
- OCaml runtime
- host OS, filesystem, binary loader, and CPU
- the process that invokes `rigor-kernel-checker`

Explicitly not in this cone: C++ consensus, full node, network,
wallet, database, pool, cryptographic primitive implementations.

See `BoundaryCorrection.md` and `ExtractionBoundary.md`.
