# Formal claims

**Status:** Public claim boundary (index).  
**SSOT:** [`../../audit/formal_bridge/ReleaseClaimBoundary_v1.md`](../../audit/formal_bridge/ReleaseClaimBoundary_v1.md)

```
REFINEMENT_PROVED=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
```

This page does not invent a maturity number. `MATURITY_LEVEL` is emitted
only from `formal_verification/rigor_kernel/release/FinalProofInventory.md`.

## YES

```
Coq consensus kernel model has formally verified invariants.
```

Full public sentence:

> Formally verified critical consensus invariants inside a Coq consensus
> kernel model, with explicit extraction and runtime trust boundaries.

## NO

```
RIGOR node implementation is formally verified.
C++ ConnectBlock refinement proved.
compiler correctness proved.
Level 5 achieved.
```

Also not allowed: `HaveCoin` / `DecodeBlock` / `EvaluateConnect`
“verified”, extracted kernel as production, SHA/Ed25519 implementation
verified.

See [`VerificationStatus.md`](./VerificationStatus.md).
