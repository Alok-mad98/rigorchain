# Formal verification status

**Status:** Public formal boundary (index).  
**Claim SSOT:** [`../../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`](../../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md)

```
MATURITY_LEVEL=4.9
REFINEMENT_PROVED=false
CPP_KERNELINPUT_EQUIVALENCE=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
equivalence_claimed=false
```

4.9 is not Level 5. Do not write “almost 5” as a claim.

Formal verification proves properties of the protocol **model**. This
repository does not claim that production C++ is a refinement of that
model.

## Allowed public sentence

> Formally verified critical consensus invariants inside a Coq consensus
> kernel model, with explicit extraction and runtime trust boundaries.

## Forbidden sentences

- production C++ node verified
- ConnectBlock refinement proved
- compiler correctness proved
- Level 5 achieved
- `CPP_VERIFIED`
- `LEVEL_5` as a current achievement

## Where to read more

| Document | Role |
|----------|------|
| [`Claims.md`](./Claims.md) | Yes / no release sentences |
| [`../../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`](../../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md) | Status SSOT |
| [`../../audit/formal_bridge/ReleaseClaimBoundary_v1.md`](../../audit/formal_bridge/ReleaseClaimBoundary_v1.md) | Claim boundary |
| [`../../formal_verification/rigor_kernel/release/FinalProofInventory.md`](../../formal_verification/rigor_kernel/release/FinalProofInventory.md) | Emitted `MATURITY_LEVEL` |
| [`../security/HOW_TO_VERIFY_RIGOR.md`](../security/HOW_TO_VERIFY_RIGOR.md) | How to reproduce |
| [`../../formal_verification/README.md`](../../formal_verification/README.md) | Operator map for the Coq tree |

Historical L2 / P0 refinement reports are in
[`../../archive/formal_history/`](../../archive/formal_history/).
They are not current claims.
