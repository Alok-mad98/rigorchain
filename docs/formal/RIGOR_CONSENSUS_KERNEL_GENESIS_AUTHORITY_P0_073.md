# Consensus Kernel genesis authority (P0-073)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-GENESIS-AUTHORITY-FINALIZATION-P0-073`  
**Date:** 2026-08-31  
**Nature:** Genesis-time authority finalization. Mainnet has not launched.

```
CONSENSUS_KERNEL_GENESIS_AUTHORITY = FINALIZED
SINGLE_PRODUCTION_AUTHORITY        = TRUE
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                  = TRUE
EQUIVALENCE_PROVED                 = FALSE
MAINNET_READY                      = TRUE
```

There is no historical chain, no legacy blocks, and no backward-compatibility
requirement. This is not a live-chain migration.

---

## Production path

```
ConnectBlock
    → prepare Kernel input          (state assembly only)
    → RigorConsensusKernel          (unique consensus authority)
    → Transaction Kernel / spend
    → consume result
    → apply UTXO / index state
```

ConnectBlock does not calculate subsidy, maturity, draw, asset class, or
ledger delta. Unique formula bodies stay in:

| Domain | Authority |
|--------|-----------|
| ORU issuance | `GetBlockSubsidy` / `ComputeRigorIssuance` |
| RES schedule | `GetResMintReward` |
| Draw | `ResolveConvergenceDraw` via `RigorConsensusKernel::ResolveDraw` |
| RES mint | `ConnectEvaluateCoinbaseResMint` via `EvaluateConnect` |
| Supply | `RigorSupplyDelta` via `EvaluateConnect` |
| Classify | `ClassifyRigorTxout` |
| Spend | `VerifyRigorPredicate` |

---

## Extracted kernel role (Option B)

Production authority is the handwritten C++ Consensus Kernel, called through
`RigorConsensusKernel`.

Coq extraction in this tree is OCaml. Production does **not** link an OCaml
runtime. There is no generated C kernel from Coq, so Option A (extracted spec
→ generated C/C++ callable kernel) is not genesis-ready.

The C ABI (`consensus/bridge`) remains a verify-only portable surface of the
same C++ functions and is not compiled into `bitcoind`.

---

## Isolated from production

Shadow, canary, and dual-path compare remain as **test / audit / fuzz**
tooling only. They are not on the ConnectBlock decision path. The operator
flag `-enableconsensuskernelcanary` is not registered in production.

---

## Formal honesty

- `COQ_ADMITTED = 0`, `COQ_AXIOMS = 0`, `COQ_PARAMETERS = 0`
- `REFINEMENT_PROVED = TRUE` (existing Kernel refinement, unchanged)
- `EQUIVALENCE_PROVED = FALSE` (do not claim C++ = Coq = Python)

Manifest: `formal_verification/genesis_consensus_authority_manifest.json`
