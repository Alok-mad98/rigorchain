# Formal Cone Map

Level 4.9 freeze. Map of the Coq consensus kernel cone only.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

```
Specification
        ↓
Abstract Kernel
        ↓
Concrete Kernel
        ↓
Unified Kernel State
        ↓
Kernel Transition
        ↓
Executable Kernel Model
        ↓
Extraction Interface
        ↓
Runtime Semantic Model
        ↓
Checker Contract
```

Each layer: **PROVED** (Coq model), **NOT PROVED**, **TCB**.

## Specification

| | |
|---|---|
| PROVED | Declared kernel invariants and machine-checked non-claims (`Audit/KernelNonClaims.v`) |
| NOT PROVED | That the specification matches Bitcoin / RIGOR production text |
| TCB | Human reading of `release/PublicClaims.md` / `VerificationScope.md` |

## Abstract Kernel

| | |
|---|---|
| PROVED | Category-A domain invariants (UTXO, ORU, RES schedule, script subset, abstract PoW/ASERT) |
| NOT PROVED | Script VM, signatures, concrete hash, ConnectBlock |
| TCB | Coq 8.15.0 checking the abstract `.v` files |

## Concrete Kernel

| | |
|---|---|
| PROVED | `concrete_step_refines_abstract_step`, `concrete_state_preserves_invariant` |
| NOT PROVED | C++ layout, node state, OP-R2 implementation refinement |
| TCB | Same Coq kernel; concrete records are still Gallina |

## Unified Kernel State

| | |
|---|---|
| PROVED | `KernelState` / `kernel_invariants` bundle |
| NOT PROVED | That a live node UTXO set is this record |
| TCB | Coq record definitions |

## Kernel Transition

| | |
|---|---|
| PROVED | `kernel_step_preserves_invariants`, `kernel_transition_deterministic`, value-flow bridge |
| NOT PROVED | Liveness, k-deep / honest-majority (OP-R1), fairness. RES rewind and most-work selection are proved separately |
| TCB | Coq `kernel_step` / `kernel_apply` |

## Executable Kernel Model

| | |
|---|---|
| PROVED | `execution_projection_preserves_invariants`, `executable_step_refines_concrete_step`, determinism, Coq-model `decode_encode_identity` |
| NOT PROVED | ocamlopt, ABI, CPU, native binary |
| TCB | Coq executable records + host that runs `coqc` |

## Extraction Interface

| | |
|---|---|
| PROVED | `extraction_interface_bijective`, `operation_trace_preserved`, source identity (`extraction_source_equivalence`, Category B) |
| NOT PROVED | Gallina → OCaml operational correspondence; OCaml compiler correctness |
| TCB | Coq `Extraction` mechanism (trusted, not proved) |

## Runtime Semantic Model

| | |
|---|---|
| PROVED | `runtime_projection_preserves_kernel_domain`, `runtime_step` refines executable step, failure preservation, kernel-domain integer bounds |
| NOT PROVED | OCaml runtime, OS, CPU, native binary |
| TCB | Host runtime that executes `coqc` / extracted checker (unproved) |

## Checker Contract

| | |
|---|---|
| PROVED | Abstract `checker_accept_contract` / `checker_reject_contract` |
| NOT PROVED | `rigor-kernel-checker` binary ≡ Coq contract; ConnectBlock |
| TCB | Abstract Coq interface (Layer 6). The extracted binary is **outside** this layer |

## Stop line

Proof stop is the Coq runtime / checker **model**. Compiler, ABI, CPU,
and native binary remain OPEN (`COMPILER_CORRECTNESS=false`,
`BINARY_SEMANTIC_EQUIVALENCE=false`).

Companion: `release/TCBInventory.md`, `release/RuntimeBoundaryClosureReport.md`.
