# Consensus Kernel performance validation (P0-066)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PERFORMANCE-VALIDATION-P0-066`  
**Date:** 2026-08-31  
**Nature:** Production-path performance baseline on already CLOSED kernels. No consensus edit.

```
CONSENSUS_KERNEL_PERFORMANCE     = PASS
MEMORY_SAFETY_BASELINE           = PASS
NO_CONSENSUS_REGRESSION          = PASS
PRODUCTION_CONSENSUS_MODIFIED    = NO
```

---

## Production path (unchanged)

```
ConnectBlock
    |
    existing C++ kernel
    |
    UTXO mutation
```

Issuance, RES schedule, draw, supply, PoW, and ASERT were not modified. ConnectBlock was not switched onto a new executor.

---

## What is measured

| Bench | Legacy path | Kernel-boundary path |
|---|---|---|
| ConsensusKernel | `GetBlockSubsidy` + `GetResMintReward` + `ResolveConvergenceDraw` + `RigorSupplyDelta` | `ComputeRigorIssuance` + same draw / supply |
| TransactionKernel | `ClassifyRigorTxout` / `IsResMintScript` / `IsWitnessProgram` | `VerifyRigorPredicateBoundary` |
| LedgerTransition | sequential issuance + draw + predicate + `LedgerApply` | same ledger apply |
| UTXOStateTransition | sequential composition above | `UTXOStateTransition` single entry |

Legacy calls are the ConnectBlock-shaped orchestration of **existing** authorities. The kernel path does not recode formulas.

---

## Scenarios

1. Empty transaction block (genesis / empty Δ)
2. Ordinary ORU transfer
3. RES mint maturity (lock vs one block early)
4. Convergence draw block
5. Large UTXO snapshot
6. Many-input transaction
7. Many-output transaction
8. Malformed rejection

---

## Metrics

Recorded per `(scenario, path)`:

- `ops_per_sec` (blocks/sec or tx/sec for that fixture)
- `latency_ns` (kernel latency)
- `rss_kb` / `peak_rss_kb`
- `alloc_bytes` (glibc `mallinfo2` live heap delta)
- `fingerprint` (xor-fold of accept / winner / Δ size)

Runner rejects:

- missing scenario
- fingerprint mismatch across two runs (nondeterminism)
- kernel latency above `max(16× legacy, legacy + 250µs)` (consensus latency regression)
- RSS growth above 32 MiB on the ledger leak probe
- fuzz crash

---

## Artifacts

| Path | Role |
|---|---|
| `benchmarks/consensus_kernel_benchmark.cpp` | Consensus Kernel |
| `benchmarks/transaction_kernel_benchmark.cpp` | Transaction Kernel |
| `benchmarks/ledger_transition_benchmark.cpp` | Ledger + UTXO State Transition |
| `tools/run_consensus_performance.py` | compile / run / compare |
| `fuzz/utxo_state_transition_perf/` | random valid / invalid / large UTXO |

```bash
python3 tools/run_consensus_performance.py
bash fuzz/utxo_state_transition_perf/run_smoke.sh
```

---

## Honesty

This is a **performance baseline**, not a complexity proof and not a full-node ConnectBlock wall-clock study. `EQUIVALENCE_PROVED` remains false. Production still uses handwritten C++ kernels; extracted OCaml is not linked.
