# Consensus Kernel Single Authority Closure (P0-054)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-SINGLE-AUTHORITY-CLOSURE-P0-054`  
**Date:** 2026-08-30  
**Nature:** Authority closure. Not a protocol change. Not a production switch.

```
CONSENSUS_KERNEL_SINGLE_AUTHORITY = CLOSED
PRODUCTION_KERNEL_PATH            = SINGLE
EXTRACTED_KERNEL_ROLE             = VERIFICATION_ORACLE
C_ABI_BOUNDARY                    = STABLE
DUPLICATE_FORMULA_CHECK           = PASS
CONNECTBLOCK_ORCHESTRATION        = PASS
COQ_ADMITTED                      = 0
COQ_AXIOMS                        = 0
COQ_PARAMETERS                    = 0
PRODUCTION_CONSENSUS_MODIFIED     = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                 = false
EQUIVALENCE_PROVED                = false
```

```
Canonical Coq Kernel
        → Extracted Kernel Oracle
        → C ABI Boundary (verify-only)
        → Production C++ Consensus Kernel
        → ConnectBlock
```

ConnectBlock stays on handwritten C++. Extracted OCaml is not on the consensus path.

---

## Production path (single)

| Surface | Unique body | Callers |
|---|---|---|
| epoch | `RigorIssuanceEpoch` | `ComputeRigorIssuance` |
| ORU | `GetBlockSubsidy` | `ComputeRigorIssuance`, `RigorSupplyDelta` |
| RES schedule | `GetResMintReward` | `ComputeRigorIssuance`, `RigorSupplyDelta`, mint eval |
| pack | `ComputeRigorIssuance` | C ABI / tests |
| supply | `RigorSupplyDelta` | ConnectBlock |
| draw | `ResolveConvergenceDraw` | ConnectBlock after assembly |
| classify | `ClassifyRigorTxout` | `ClassifyRigorAsset` / `AssetTypeFromScriptPubKey` / `Solver` |

`GetConvergenceDrawSetForBlock` assembles `F`. It does not resolve a winner.

---

## C ABI

`core/src/consensus/bridge/` is POD + Kernel calls, compile-flag
`-DRIGOR_KERNEL_ABI_VERIFY_ONLY`. Not in `Makefile.am`.

---

## Drift (registered, not deleted)

- **Phase5Equivalence:** `NON_AUTHORITATIVE`. VACUOUS-PLACEHOLDER remains history.
- **legacy_account_model:** still `DEFAULT_BUILD` in protocol-core. Target `DEAD_BUILD` after import migration.

---

## FINAL_VERDICT

`CONSENSUS_KERNEL_SINGLE_AUTHORITY_CLOSED`
