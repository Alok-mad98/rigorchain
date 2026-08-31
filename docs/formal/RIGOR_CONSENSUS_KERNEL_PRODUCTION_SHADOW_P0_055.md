# Consensus Kernel Production Shadow (P0-055)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PRODUCTION-SHADOW-P0-055`  
**Date:** 2026-08-30  
**Nature:** Verify-only shadow execution. Not a production switch. Not a proof.

```
SHADOW_EXECUTION                  = YES
PRODUCTION_PATH                   = CPP
EXTRACTED_ROLE                    = ORACLE
CONSENSUS_KERNEL_SHADOW_STATUS    = CLOSED
CPP_KERNEL_EXECUTION              = PASS
EXTRACTED_KERNEL_EXECUTION        = PASS
SHADOW_COMPARE                    = PASS
DISCREPANCY                       = NONE
PRODUCTION_KERNEL_SWITCHED        = NO
PRODUCTION_CONSENSUS_MODIFIED     = NO
COQ_ADMITTED                      = 0
COQ_AXIOMS                        = 0
REFINEMENT_PROVED                 = false
EQUIVALENCE_PROVED                = false
```

ALIGNMENT ≠ EXTRACTION ≠ C ABI ≠ SHADOW ≠ REFINEMENT ≠ EQUIVALENCE ≠ PRODUCTION SWITCH.

---

## Production path (unchanged)

```
ConnectBlock
    → state assembly
    → handwritten C++ Consensus Kernel
    → UTXO mutation
```

Unique production bodies remain:

| Surface | Authority |
|---|---|
| epoch | `RigorIssuanceEpoch` |
| ORU | `GetBlockSubsidy` |
| RES schedule | `GetResMintReward` |
| pack | `ComputeRigorIssuance` |
| classify | `ClassifyRigorTxout` |
| draw | `ResolveConvergenceDraw` |
| supply | `RigorSupplyDelta` |

`GetConvergenceDrawSetForBlock` is state assembly. Shadow does not call it.

The extracted kernel is not linked into `bitcoind`. OCaml is not on the consensus path.

---

## Shadow boundary

`core/src/consensus/shadow/ConsensusKernelShadowExecutor` receives a prepared
`ConsensusKernelInput` only:

- height / issuance params
- candidate set `F`, target `T`, σ inputs already in the draw context
- classification `scriptPubKey` bytes
- supply-delta fields

It does not read chainstate, coinsdb, `validation.cpp`, wallet, RPC, miner, or P2P.

Compile flags: `-DRIGOR_KERNEL_SHADOW_VERIFY_ONLY` and
`-DRIGOR_KERNEL_ABI_VERIFY_ONLY`. Not listed in `Makefile.am`.

---

## Execution model

```
same ConsensusKernelInput
        │
        ├── C++ Kernel          → output_cpp
        └── extracted oracle    → output_ext
                (via C ABI types / CLI; OCaml process, not ConnectBlock)

compare: output_cpp == output_ext
```

On mismatch the executor / runner **STOP**s and prints
`CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND` with the input vector, C++ output,
and extracted output. There is no auto-repair.

In-process C ABI compare checks marshalling (C ABI still calls the C++ Kernel).
The extracted oracle compare is the OCaml CLI.

---

## Layers that stay distinct

- `GetResMintReward(0) = 40000000` (schedule). Genesis **actual** RES issuance is 0.
- C++ `RigorSupplyDelta.oru_circulating_delta` maps through `LedgerDeltaBridge`
  to Coq `ledger_oru_delta` (`subsidy − converge`). It is not Coq circulating
  (`subsidy − converge − burn`).

---

## Honesty

Finite prepared-input compare. Not a ∀-proof. Not refinement. Not equivalence.

```
REFINEMENT_PROVED = false
EQUIVALENCE_PROVED = false
```
