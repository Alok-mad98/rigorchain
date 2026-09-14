# RigorKernel — Critical Consensus Kernel (Phase A live)

**Phase A (P0-107-A) is implemented. ASERT, Keccak, Ed25519, C++ equivalence,
and full-chain validity are out of scope.**

Architecture: `docs/formal_verification/RIGOR_CRITICAL_CONSENSUS_FORMALIZATION_PLAN_P0_106.md`

Public claim: **"Formally verified critical consensus invariants (Coq consensus kernel only)."**
`full_chain_equivalence = false`. `cpp_equivalence = false`.
`extraction_equivalence = false`. `binary_semantic_equivalence = false`.

## Live Phase A modules

```
Core/Bytes.v              Amount.v              Height.v
Script/ScriptTypes.v      ScriptClassifier.v
Supply/ORUModel.v         ORUConservation.v
Pow/AbstractHash.v        PowModel.v
```

## Build

```
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make
```

Coq 8.15.0. No external axioms. This `_CoqProject` does **not** `-R` the existing
`formal_verification/coq` tree (no sha3 / keccak / crypto import).

## Hard rules

- No `Admitted`, `admit`, `Axiom`, or `Parameter` in the proof cone.
- Do **not** define `refinement_proved` or `equivalence_proved` booleans.
- Do **not** state a `forall header` PoW theorem whose proof unfolds Keccak.
- [H] in `Pow/AbstractHash.v` is an uninterpreted `list byte -> list byte`.
