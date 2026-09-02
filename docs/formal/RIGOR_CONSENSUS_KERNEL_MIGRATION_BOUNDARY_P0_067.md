# Consensus Kernel production migration boundary (P0-067)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PRODUCTION-MIGRATION-READINESS-P0-067`  
**Date:** 2026-08-31  
**Nature:** Migration readiness only. No production switch. No protocol edit.

```
CONSENSUS_KERNEL_MIGRATION_READINESS = CLOSED/PASS
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
DUPLICATE_AUTHORITY                  = NONE
MIGRATION_BOUNDARY                   = READY
REFINEMENT_PROVED                    = TRUE
EQUIVALENCE_PROVED                   = FALSE
COQ_ADMITTED                         = 0
COQ_AXIOMS                           = 0
COQ_PARAMETERS                       = 0
```

---

## CURRENT

```
ConnectBlock
    |
    handwritten authority
    |
    +-- GetBlockSubsidy
    +-- GetResMintReward
    +-- ComputeRigorIssuance
    +-- ResolveConvergenceDraw
    +-- ClassifyRigorTxout
    +-- VerifyRigorPredicate
    +-- RigorSupplyDelta
    +-- existing UTXO mutation
```

This remains the production path.

---

## READY (switch OFF)

```
ConnectBlock
    |
    v
ConsensusKernelInput construction
    |
    v
ConsensusKernel
TransactionKernel
LedgerKernel
    |
    v
KernelResult
    |
    v
existing DB / UTXO mutation
```

The shim under `core/src/consensus/kernel_migration/` adapts ConnectBlock-shaped
inputs and results. It may only **call** the authorities above. It does not
recode issuance, RES, draw, classify, sighash, verify, or supply.

```
EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
```

`validation.cpp` does not reference the shim. `Makefile.am` does not compile
`consensus/kernel_migration`. Extracted OCaml is not linked.

---

## Dual execution (test-only)

Flag: `CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY`

```
A: current production authority path
B: Kernel boundary path (ComputeRigorIssuance / UTXOStateTransition / LedgerApply)
```

Compared: issuance, maturity, draw winner, classification, spend, ledger
accept, coinbase / supply reject reason.

Mismatch: **FAIL HARD**. Never auto-correct.

Logging hooks compile out unless `KERNEL_MIGRATION_LOG=1`.

---

## Vectors

`formal_verification/kernel_migration_vectors.json`

Genesis (height 0): RES schedule 40000000, actual issuance 0, draw none.  
Epochs: 1, 99, 100, 101, 209999, 210000, 210001, 419999.  
Transaction / draw / ledger cases as listed in that file.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing kernel-boundary refinement
(P056–P065). This TASK does not add a new proof.
`EQUIVALENCE_PROVED = FALSE`: not a full-node proof. Production remains
handwritten C++.
