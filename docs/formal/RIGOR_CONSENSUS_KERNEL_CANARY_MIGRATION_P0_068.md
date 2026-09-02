# Consensus Kernel canary migration (P0-068)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-CANARY-MIGRATION-P0-068`  
**Date:** 2026-08-31  
**Nature:** Rollback-ready dual-path canary. No production switch. No equivalence claim.

```
CANARY_IMPLEMENTED     = TRUE
CPP_PATH_STATUS        = PASS
EXTRACTED_PATH_STATUS  = PASS
COMPARE_STATUS         = PASS
DISCREPANCY_COUNT      = 0
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
EQUIVALENCE_PROVED     = FALSE
```

---

## Migration architecture

```
ConnectBlock  ── production default ──► handwritten C++ authority
                                         (unchanged)

prepared ConsensusKernelInput
        |
        +-- PATH_A: existing C++ authority
        |     GetBlockSubsidy / GetResMintReward / ComputeRigorIssuance
        |     ResolveConvergenceDraw / ClassifyRigorAsset
        |     RigorSupplyDelta / VerifyRigorPredicateBoundary
        |
        +-- PATH_B: stable C ABI only
        |     rigor_compute_issuance / rigor_classify_script
        |     rigor_resolve_draw / rigor_supply_delta / rigor_coinbase_mature
        |
        v
   compare KernelResult
        |
        mismatch ► CONSENSUS_KERNEL_CANARY_DISCREPANCY  (FAIL HARD)
```

PATH_B enters the extracted-kernel surface only through the existing POD C ABI
(`core/src/consensus/bridge`). The canary does not link OCaml, `caml_startup`,
GC, or extracted `.ml`. Spend still has no C ABI export; both paths call the
existing `VerifyRigorPredicateBoundary` (no new parser).

The canary does not recode subsidy, RES, draw, classify, supply, or parse.

---

## Rollback path

Flag: `-DENABLE_CONSENSUS_KERNEL_CANARY`

| Build | Flag | Path |
|---|---|---|
| Production release | **OFF** (default) | ConnectBlock → handwritten C++ only |
| Canary / test | ON | Dual-exec + compare; production path unused |

Rollback is compile-time: omit the flag. `Makefile.am` does not list
`consensus/kernel_canary`. `validation.cpp` does not reference the executor.
`test_consensus_kernel_canary.cpp` is not in `test_rigor`.

Mismatch never auto-corrects. The handwritten path remains the only production
authority.

---

## Production default OFF

```
EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
```

This TASK does not flip ConnectBlock. It only opens a reversible compare
channel.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing kernel-boundary refinement
(P056–P067). This TASK does not add a new proof.

`EQUIVALENCE_PROVED = FALSE`: the canary is a finite-vector compare, not a
full-node equivalence proof.
