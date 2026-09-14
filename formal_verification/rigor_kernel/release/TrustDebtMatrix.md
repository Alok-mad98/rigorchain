# Trust Debt Matrix

What is claimed today versus what a stronger claim would still need.
This matrix does **not** authorize a stronger public claim.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

Difficulty: **LOW** = local Coq corollary / docs. **MEDIUM** =
dedicated engineering. **HIGH** = CompCert / seL4 scale.

Recommendation: **keep** = stay at current honesty.
**possible** = only with a new workstream and Closed theorems.
**do-not** = not the next increment of *this* cone.

| Item | Current claim | Missing proof | Estimated difficulty | Recommendation |
|---|---|---|---|---|
| Coq → extracted OCaml refinement | Source identity only (`extraction_source_equivalence`) | Gallina–OCaml operational correspondence; integer mapping | HIGH | do-not as P0; possible as a new cone |
| OCaml compiler correctness | `COMPILER_CORRECTNESS=false` | CompCert/CakeML proof about `ocamlopt`, or a switched verified backend | HIGH | do-not until a Clight generator exists |
| Binary semantic equivalence | `BINARY_SEMANTIC_EQUIVALENCE=false` | Compiler + extraction + linker correspondence | HIGH | do-not |
| C++ consensus refinement | Differential MATCH on a comparable fragment; `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE` | Coq `check_*` ⇔ named `ConnectBlock` (scripts, sigs, keccak, full ASERT nBits) | HIGH | possible (fragment growth only) |
| Full Bitcoin / RIGOR node refinement | Forbidden public wording | Entire node, P2P, mempool, RPC, storage | HIGH | do-not |
| Crypto primitive security | Assumptions, not theorems | Collision / EUF-CMA proofs or verified impls | HIGH | keep as assumptions |
| Reorg / common-prefix | OP-R1 Still open | Fork-choice automaton + honesty assumption | HIGH | do-not as kernel Category A |
| Network liveness | OP-L1…L3 Not formalized | Scheduler / partial-synchrony model | HIGH | do-not as this cone |
| Script VM | Classifier only (PARTIAL) | Opcode interpreter + sighash + Ed25519 | MEDIUM–HIGH | possible as a *separate* cone |
| Local schedule / UTXO corollaries | Some Closed (OP-X1…X3) | Further intra-model lemmas | LOW | keep optional; do not inflate Category A |
| ORU vs RES substitution | Separately modelled (`res_creation_separate_from_oru_transfer`) | Production dual-pool ConnectBlock | MEDIUM | keep; ORU Domain = Independent Verified Domain |
| Unified Kernel Transition | Model `kernel_step` preserves the invariant bundle and is deterministic | C++ ConnectBlock / full node / binary correspondence | HIGH | keep; Coq model only |
| Concrete implementation refinement (OP-R2) | Coq abstract → Coq concrete (`concrete_step_refines_abstract_step`) | C++ / node / compiler correspondence | HIGH | keep; see `ConcreteRefinementBoundary.md` |
| Executable / compiler / ABI | Coq concrete → executable model Closed | Compiler, ABI, CPU, C++ | HIGH | keep; Level 4.5 model boundary only |
| Extraction Debt | **LOW** — extraction interface formally described (`extraction_interface_bijective`) | Compiler correctness remains OPEN | HIGH (compiler) | keep; interface Closed, compiler not |
| Runtime Debt | **LOW** — Coq runtime abstraction Closed (`runtime_projection_preserves_kernel_domain`) | OCaml runtime, ABI, CPU, native binary | HIGH | keep; Coq model only |

`TRUST_DEBT_MATRIX=TRUE`
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
