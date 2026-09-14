# P0-119 Decision Report

Inventory close-out. No new consensus proof. No public-claim change.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

Maps: `KernelInventory.md`, `TCBInventory.md`, `TrustDebtMatrix.md`,
`OpenProperties.md`.

---

## Question 1: Is current formal verification complete?

Complete for declared kernel invariant scope.

---

## Question 2: Can public claim be strengthened?

**NO**

Category A is Closed for the declared Coq-model invariants. Extraction
is source identity only. Compiler, binary, C++, network, wallet, and
crypto security remain unproved. Strengthening the sentence would
overclaim.

---

## Question 3: Should we continue toward theorem→binary?

| Path | Class |
|---|---|
| Keep the declared kernel scope honest; optional LOW local corollaries; do not inflate Category A | **Recommended** |
| Larger comparable-fragment differential tests (still not a refinement proof) | **Possible** |
| Verified-compiler path *after* license + Clight generator + correspondence theorem (`VerifiedCompilerPath.md` checklist) | **Possible** |
| Full extraction refinement (Gallina → OCaml semantics) | **Not recommended** as the next increment of this cone |
| Claim theorem→binary or switch TCB to CompCert without the checklist | **Not recommended** |
| Full-node / Bitcoin refinement | **Not recommended** |
| Network liveness / reorg as Category A of the existing checkers | **Not recommended** |

Theorem→binary is **not** a missing checkbox on the current cone. It
is a different project. Flags stay false until Closed theorems exist.

`P0-119_DECISION=RECORDED`
`P0-119_INVENTORY_COMPLETE=TRUE`
