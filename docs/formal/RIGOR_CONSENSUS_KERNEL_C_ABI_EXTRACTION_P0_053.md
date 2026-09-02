# Consensus Kernel C ABI Extraction Bridge (P0-053)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-C-ABI-EXTRACTION-BRIDGE-P0-053`  
**Date:** 2026-08-30  
**Nature:** Stable POD C ABI around the production Kernel. Not a production switch.

```
CONSENSUS_KERNEL_C_ABI_STATUS     = PASS
EXTRACTED_KERNEL_C_ABI            = PASS
CPP_ADAPTER                       = PASS
DUPLICATE_FORMULA_CHECK           = PASS
COQ_ADMITTED                      = 0
COQ_AXIOMS                        = 0
PRODUCTION_CONSENSUS_MODIFIED     = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                 = false
EQUIVALENCE_PROVED                = false
EXTRACTED_KERNEL_ABI_READY        = YES
```

ALIGNMENT ≠ EXTRACTION ≠ C ABI ≠ REFINEMENT ≠ PRODUCTION SWITCH.

---

## Bridge

```
Coq canonical kernel
        |
        v
 extracted executable kernel   (consensus_kernel_c.ml)
        |
        v
 stable C ABI boundary         (consensus/bridge, POD only)
        |
        v
 C++ consensus kernel adapter  (calls production Kernel)
        |
        v
 ConnectBlock                  (unchanged; still handwritten C++)
```

`EXTRACTED_KERNEL_ABI_READY = YES` means the ABI exists and is tested.
It does not mean ConnectBlock calls it.

---

## C ABI (POD)

`uint256` is a C++ class, so the C header uses `rigor_u256` (32 raw bytes).
No `std::vector`, `std::string`, `CTransaction`, `CBlockIndex`, or `CCoinsView`
appears in `kernel_types.h` / `consensus_kernel_c_api.h`.

| ABI | Production Kernel call |
|---|---|
| `rigor_compute_issuance` | `ComputeRigorIssuance` |
| `rigor_classify_script` | `ClassifyRigorAsset` |
| `rigor_resolve_draw` | `ResolveConvergenceDraw` |
| `rigor_supply_delta` | `RigorSupplyDelta` |
| `rigor_coinbase_mature` | `Params.nCoinbaseMaturity` |

Adapters do not recode `GetBlockSubsidy`, `GetResMintReward`,
`ResolveConvergenceDraw`, `ClassifyRigorAsset`, or `RigorSupplyDelta`.

Draw still requires C, B_{h-1}, payout height, and T. Those are Kernel
inputs, not `GetConvergenceDrawSetForBlock` (state assembly).

Supply `delta` is the C++ ledger field `oru_circulating_delta`
(`subsidy − converge`). It is not Coq `apply_delta` circulating
(`subsidy − converge − burn`).

---

## Extraction

`formal_verification/coq/ExtractConsensusKernelC.v` is not in `_CoqProject`.
Coq 8.15 has no `Print Extraction` vernacular; the inventory is the
`Extraction "consensus_kernel_c.ml"` list. No `Axiom` / `Parameter` /
`Admitted`. No `Extract Constant`.

---

## Verify-only

Bridge translation units require `-DRIGOR_KERNEL_ABI_VERIFY_ONLY`.
They are not listed in `core/src/Makefile.am`. ConnectBlock still calls:

```
RigorSupplyDelta
ResolveConvergenceDraw
ClassifyRigorAsset
```

---

## Tests

| Check | Result |
|---|---|
| `coqc ExtractConsensusKernelC.v` | PASS |
| `ocamlopt` extracted C kernel CLI | PASS |
| `test_consensus_kernel_c_abi.py` | PASS |
| `lint-consensus-kernel-c-api.py` | PASS |
| `run_consensus_kernel_refinement_gate.sh` | PASS |
| `run_differential_gate.sh` | PASS |

Honesty flags stay false.

---

## FINAL_VERDICT

`CONSENSUS_KERNEL_C_ABI_BRIDGE_CLOSED`
