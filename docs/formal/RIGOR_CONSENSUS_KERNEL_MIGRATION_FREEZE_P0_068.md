# Consensus Kernel migration freeze (P0-068)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-MIGRATION-FREEZE-P0-068`  
**Date:** 2026-08-31  
**Nature:** Pre-switch freeze audit. No protocol edit. No production switch.

```
CONSENSUS_KERNEL_MIGRATION_FREEZE = CLOSED
CANARY_STATUS                     = PASS
CURRENT_CPP_AUTHORITY             = PASS
MIGRATION_PATH                    = PASS
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                 = TRUE
EQUIVALENCE_PROVED                = FALSE
```

---

## Current (production)

```
Block
  |
  v
ConnectBlock
  |
  v
C++ Consensus Authority
  |
  v
UTXO mutation
```

ConnectBlock remains orchestration. Authorities stay handwritten C++.

---

## Candidate (not production)

```
Block
  |
  v
ConsensusKernel Boundary
  |
  v
Existing Authority
  |
  v
UTXO mutation
```

The candidate may only **call** the frozen authorities. It does not recode
issuance, RES, draw, classify, supply, spend, or PoW.

```
candidate != production
EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
```

`validation.cpp` and `Makefile.am` are unchanged. OCaml / `caml_startup` /
`consensus_kernel.ml` stay out of the node.

---

## Frozen unique authorities

`formal_verification/consensus_kernel_freeze_manifest.json`

| Domain | Authority |
|---|---|
| issuance | `GetBlockSubsidy` |
| res_schedule | `GetResMintReward` |
| draw | `ResolveConvergenceDraw` |
| classification | `ClassifyRigorTxout` |
| supply | `RigorSupplyDelta` |
| spend | `VerifyRigorPredicate` |
| ledger | `LedgerApply` |
| pow_boundary | `PowBoundary` |

A new authority name is `CONSENSUS_AUTHORITY_FREEZE_VIOLATION`.

`LedgerApply` is the verify-only ledger boundary (P063). Production UTXO
mutation is still ConnectBlock's existing path.

---

## Freeze compare

Within the freeze vector set:

```
current C++ authority
        =
migration candidate path
        =
existing refinement pins
```

Mismatch token: `CONSENSUS_KERNEL_FREEZE_DRIFT`. No auto-correct.

This is a finite-vector freeze, not a full-node proof.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing kernel-boundary refinement
(P056–P067). This TASK does not add a new proof.

`EQUIVALENCE_PROVED = FALSE`: freeze agreement is not equivalence.
