# Maturity Level Definition

Audit finding M-5: the package carried level statements in several
documents that did not agree, and none of them said what a level
means. This file is now the **only** place a level is defined, and
`release/FinalProofInventory.md` is the only place one is asserted
(as `MATURITY_LEVEL=`, emitted by the generator). Any other document
quoting a number is stale and CI should be taught to reject it.

A level here describes **how much of the distance between a paper
specification and a running node has been formally closed**. It is not
a security rating, not a certification, and not comparable to EAL or
SIL numbers.

## Scale

| Level | Means |
|---|---|
| 1 | Properties written in prose; no machine-checked artefact |
| 2 | Core invariants stated in a proof assistant; proofs incomplete or admitted |
| 3 | Core invariants proved, no admits, no axioms; single abstraction level |
| 4 | Layered model with refinement between levels; mutation-tested; extraction interface declared |
| 4.5 | Refinement obligations are non-trivial (lower layer has semantics the upper layer does not); decision procedures proved sound *and* complete |
| 4.9 | As 4.5, plus a runtime domain with its own failure modes, a reproducible source lock, and classification governance over the whole cone |
| 5 | Extracted code proved to refine the model, and the compiler in the TCB is itself verified |

## Why this package is 4.9 and not 5

Level 5 requires two things this package explicitly does not claim:

1. **Extraction correctness.** `kernel_call_boundary` is proved to be a
   bijection onto the declared extracted entries
   (`extraction_interface_injective`, `extraction_interface_surjective`).
   That is interface consistency. It is not a proof that the OCaml
   `ExtractedKernel.ml` computes what the Coq function computes.
   `EXTRACTION_FORMAL_PROOF=source_identity_only`.

2. **Verified compiler.** `verified_compiler_in_current_tcb = false`.
   `ocamlopt` is trusted, not verified. `COMPILER_CORRECTNESS=false`,
   `BINARY_SEMANTIC_EQUIVALENCE=false`.

Both gaps are recorded in `release/OpenProperties.md` and priced in
`release/FormalizationFrontier.md`.

The CertiCoq research sidecar (Phase 0–8) generated boxed-`Z`
Clight and compiled it with sidecar `ccomp`. That does **not**
close either Level-5 gap: OCaml extraction is still
source-identity only, and no verified compiler entered the
checking TCB. `PHASE7_PIPELINE_INSTANTIATED=false`.
Maturity stays **4.9**.

## Why 4.9 and not 4.5

The three things added past 4.5:

- **A runtime domain with its own failure modes.** `Runtime/MachineState.v`
  is not a renaming of the executable model: numeric fields live in a
  63-bit checked-word domain, `machine_decode` is partial, and
  `machine_step_of` fails on overflow rather than widening. The
  refinement obligation can therefore fail, and
  `machine_refinement_is_partial` exhibits a state where it does.

- **A reproducible source lock.** `release/SourceManifest.sha256` is
  written by a freeze that refuses to run unless generated documents
  are already reproducible, snapshots the tree before and after, and
  verifies its own output. Before the P0-130 audit the manifest
  recorded a hash of a file that exists in no commit.

- **Classification governance over the whole cone.** The reverse scan
  in `scripts/check_theorem_classification.py` used to cover two
  directories; it now covers all of them. It also used to read only
  `Theorem` and `Corollary` at column zero, which left every `Lemma`
  outside classification — 210 of the cone's 713 statements, 69 of
  them used inside a Category A proof. It reads every statement form
  now, and `Print Assumptions` runs over Category A, B, C **and** the
  allowlist, so the closure check covers the cone rather than the
  table.

## What a level explicitly does not mean

- Not "the consensus rules are correct". The kernel is a model of a
  subset of them; `release/FinalClaimsBoundary.md` lists what is in.
- Not "the node is verified". See `Audit/KernelNonClaims.v`.
- Not "the cryptography is sound". `H` is an uninterpreted function;
  `kernel_does_not_assume_sha3_security` is a theorem in this package.
