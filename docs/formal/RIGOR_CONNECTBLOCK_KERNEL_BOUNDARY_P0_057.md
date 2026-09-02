# ConnectBlock ↔ Consensus Kernel Boundary (P0-057)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-CONNECTBLOCK-BOUNDARY-P0-057`  
**Date:** 2026-08-30  
**Nature:** Formal boundary. Not a full ConnectBlock proof.

```
CONSENSUS_KERNEL_CONNECTBLOCK_BOUNDARY = CLOSED
INPUT_PREPARATION_ALIGNMENT            = PASS
RESULT_CONSUMPTION_ALIGNMENT           = PASS
CONSENSUS_LOGIC_DUPLICATION            = NONE
VALIDATION_ORCHESTRATION_ONLY          = PASS
COQ_ADMITTED                           = 0
COQ_AXIOMS                             = 0
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
PRODUCTION_CONSENSUS_MODIFIED          = NO
REFINEMENT_PROVED                      = TRUE
EQUIVALENCE_PROVED                     = FALSE
```

`Print Assumptions` on the boundary theorems is
`Closed under the global context`.

P056 kernel `refinement_proved = true` is unchanged.
This file does **not** prove ConnectBlock, UTXO, DB, PoW, or the
Transaction Kernel.

---

## Boundary

```
BlockState
    → ConsensusKernelInput     (PrepareKernelInput)
    → ConsensusKernelResult    (RunKernel)
    → ConnectBlockApply        (accept / reject / apply Δ)
```

`ConsensusKernelInput` holds height, issuance params, F, T, script bytes,
supply fields, and coinbase-expectation fields. It does not hold a UTXO
pointer, chainstate, `CBlockIndex`, RPC, wallet, or miner state.

`GetConvergenceDrawSetForBlock` remains state assembly. The Kernel object
is still `ResolveConvergenceDraw(F, T)`.

---

## Proven

| File | Theorem |
|---|---|
| `ConnectBlockInputPreparation.v` | `PrepareKernelInput = KernelInput` |
| `KernelResultApplication.v` | apply is reject or `ApplyDelta`; no formula recompute |
| `ConnectBlockKernelBoundary.v` | `ConnectBlockOrchestration = ConnectBlockCanonical` |

---

## validation.cpp audit

`GetBlockSubsidy` no longer lives in `validation.cpp` (moved in P054).
ConnectBlock calls `ResolveConvergenceDraw`, `RigorSupplyDelta`, and
`ConnectEvaluateCoinbaseResMint`. It does not copy issuance, draw,
classify, or supply formulas.

| | LOC |
|---|---|
| `validation.cpp` file | 6962 |
| Before (subsidy body that used to live in `validation.cpp`) | 35 consensus LOC |
| After (`ConnectBlock` function, orchestration) | 743 |

`PRODUCTION_CONSENSUS_MODIFIED = NO` — this TASK did not edit
`validation.cpp`.

---

## Not proven

- Full ConnectBlock
- UTXO mutation / coinsdb / LevelDB
- PoW
- Transaction signatures
- Production Kernel switch

---

## FINAL_VERDICT

`CONNECTBLOCK_CONSENSUS_KERNEL_BOUNDARY_CLOSED`
