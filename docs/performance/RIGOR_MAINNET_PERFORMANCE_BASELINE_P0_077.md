# RIGOR Mainnet Performance Baseline (P0-077)

**Task:** `TASK-RIGOR-MAINNET-PERFORMANCE-BASELINE-OPTIMIZATION-P0-077`  
**Date:** 2026-08-31  
**Nature:** Mainnet performance baseline + low-risk layout/allocation optimization.
Consensus behavior is unchanged.

This TASK does not modify Consensus Kernel semantics, Transaction Kernel
semantics, Ledger Transition semantics, issuance, RES schedule, draw, supply,
PoW, or ASERT. It does not add a second kernel, a consensus shortcut, or an
approximate calculation.

```
RIGOR_MAINNET_PERFORMANCE_STATUS = PASS
PERFORMANCE_BASELINE             = PASS
CONSENSUS_BEHAVIOR_CHANGED       = FALSE
STATE_TRANSITION_CHANGED         = FALSE
HASH_CHANGED                     = FALSE
MEMORY_SAFETY                    = PASS
CRASH_FOUND                      = NO

FINAL_VERDICT:
MAINNET_PERFORMANCE_BASELINE_CLOSED
```

Reproduce:

```bash
python3 tools/run_mainnet_performance_benchmark.py
bash tools/run_mainnet_performance_gate.sh
```

Machine-local numbers: [`performance_results.json`](./performance_results.json).

---

## Production path (unchanged)

```
ConnectBlock
    |
    RigorConsensusKernel          (unique production wrappers)
    |
    VerifyRigorPredicate          (unique spend authority)
    |
    CCoinsViewCache mutation      (production UTXO)
```

Verify-only compose (`UTXOStateTransition` / `LedgerApply`) remains **absent**
from `Makefile.am` and is not called from `validation.cpp`.

`VerifyRigorPredicateBoundary` is no longer in that set. Since A1 it is the
body of `VerifyRigorPredicate`: production marshals through `MakeKernelInput`
and takes the accept/reject verdict from the boundary, so the header is
reachable from `validation.cpp` through the unique spend authority.
`validation.cpp` still must not call the boundary directly (lint:
`tools/lint-script-authority.py`). Ed25519 trips per spend are unchanged from
the pre-A1 predicate and are pinned by invocation counts in
`test_predicate_boundary_differential`.

---

## What was measured

| Bench | Entry | Sub-steps |
|---|---|---|
| Consensus Kernel | `ComputeRigorIssuance` / draw / supply (same bodies `RigorConsensusKernel` wraps) | issuance lookup, maturity, classification, draw resolution, supply |
| Transaction Kernel | `VerifyRigorPredicateBoundary` | predicate, sighash preimage, classify, signature encoding |
| Ledger Apply | `LedgerApply` | value conservation, uniqueness, maturity |
| UTXO State Transition | `UTXOStateTransition` | apply, linear vs map input lookup |
| Block Validation | sequential compose of the above | empty / normal / large-tx / many in/out / ORU / RES / draw / large UTXO |

Scenarios: empty block, normal block, large transaction block, many-input
transaction, many-output transaction, ORU transfer, RES maturity spend,
convergence draw, large UTXO set.

---

## CPU profile (consensus-adjacent)

Bottlenecks, in order, on the verify-only compose used by this baseline:

1. **Convergence draw** with many candidates (`ResolveConvergenceDraw` /
   `Mul256x256Full` stays the unique body — not duplicated here).
2. **Large UTXO snapshot scan** in `UTXOStateTransition` (every snapshot coin
   must still be checked for `INVALID`; skipping would change semantics).
3. **Many-input uniqueness** in `LedgerApply` (was O(n²); now O(n log n) for
   n>24 via sort of a *copy*. `spent_delta` order is unchanged).
4. **Ed25519 / HASH160** on the production `VerifyRigorPredicate` path
   (boundary benches keep crypto external as `crypto_accept` / `keyhash_match`).
5. **Issuance / supply / maturity** are already O(1) arithmetic. An immutable
   last-hit issuance memo was measured in the bench only; production still
   calls `ComputeRigorIssuance` on every `RigorConsensusKernel::Issuance`.

This is a kernel-compose CPU profile, not a sampled `perf` of a live
`rigord` IBD.

---

## Memory profile

| Probe | Rule |
|---|---|
| RSS | `getrusage(RUSAGE_SELF)` after each timed loop |
| Peak RSS | max sample across benches |
| Heap delta | glibc `mallinfo2().uordblks` |
| Leak probe | 8× extra `UTXOStateTransition` on ORU transfer; fail if RSS grows > 32 MiB |

`MEMORY_SAFETY = PASS` means: no crash, no leak probe trip, dual-run
fingerprints match. It is not a proof of the whole node allocator.

---

## Latency baseline

Host: Linux WSL2 x86_64, `PERF_ITERS=600`, `-O2`. Re-run to refresh.

| Scenario | KERNEL_LATENCY | BLOCK_VALIDATION_LATENCY | UTXO_PERFORMANCE |
|---|---:|---:|---:|
| empty_block | 426 ns | 319 ns | 271 ns |
| normal_block | 445 ns | 5.2 µs | 663 ns |
| oru_transfer | 428 ns | 635 ns | 786 ns |
| res_maturity_spend | 456 ns | 1.4 µs | 2.3 µs |
| convergence_draw | 14.1 µs | 3.3 µs | 3.4 µs |
| large_transaction_block | 13.7 µs | 44.2 µs | 2.3 µs |
| many_inputs | 1.9 µs | 2.2 µs | 2.6 µs |
| many_outputs | 1.9 µs | 2.6 µs | 2.8 µs |
| large_utxo | 105 µs | 3.2 µs | 3.5 µs |

Kernel `large_utxo` here is a 64-candidate draw, not a 4096-coin snapshot (that cost shows up under UTXO apply). Peak RSS 12.7 MiB. Issuance lookup ~268 ns; last-hit memo ~2 ns (bench-only; identical pack).

Exact nanoseconds and fingerprints: `performance_results.json`. The runner rejects:

- missing scenario
- fingerprint mismatch across two runs
- fuzz crash / state drift
- regression-test result/state/hash mismatch vs the pre-optimization
  `LedgerApply` reference

---

## Optimization result

Allowed class only: allocation reduction, memory reuse, copy elimination,
immutable memo, uniqueness data-structure change with identical boolean
result.

| Change | Where | Result identity |
|---|---|---|
| Combined known-asset + ORU/RES sum (one pass) | `ledger_transition.cpp` | same accept/reject, same amounts |
| Uniqueness: linear n≤24, sort-copy n>24 | `ledger_transition.cpp` | same duplicate reject; `spent_delta` keeps consumed order |
| `CppSighashPreimage` `reserve` | `transaction_kernel_input.h` | same bytes, no hashtype byte |
| Empty `scriptPubKey` skips `CScript` | `VerifyRigorPredicateBoundary` | still reject |
| Move ledger deltas into `UTXOStateTransition` result | `utxo_state_transition.h` | same vectors |

Not done (would violate the TASK):

- formula rewrite or approximate draw/issuance
- cache that can return a different pack than `ComputeRigorIssuance`
- production parallel consensus
- `ConnectBlock` switch onto `LedgerApply` / `UTXOStateTransition`

---

## Consensus safety proof (engineering, not Coq ∀)

| Gate | Meaning |
|---|---|
| `tools/lint-performance-consensus-boundary.py` | no second authority, no formula copy, no shortcut in baseline artifacts; production Makefile still excludes verify-only kernels |
| `core/src/test/test_performance_consensus_regression.cpp` | optimized `LedgerApply` = P063 reference; dual-run state/hash identity; sighash bytes stable |
| `fuzz/performance_boundary/` | huge block / huge tx count / large UTXO / malformed; no crash; two applies fold equal |
| `lint-consensus-authority.py` + ledger/UTXO boundary lints | unique production bodies unchanged |
| differential + refinement gates | existing golden / kernel refinement still pass |

`EQUIVALENCE_PROVED` remains false. This TASK does not claim a Coq proof of
the allocation edits.

---

## Honesty

This is a **performance baseline**, not a complexity proof and not a full-node
ConnectBlock wall-clock study of IBD. Parallel signature verification is
analyzed in [`RIGOR_PARALLEL_VALIDATION_ANALYSIS.md`](./RIGOR_PARALLEL_VALIDATION_ANALYSIS.md)
and is **not** introduced. Database consistency rules are unchanged; see
[`RIGOR_DATABASE_PERFORMANCE_BASELINE.md`](./RIGOR_DATABASE_PERFORMANCE_BASELINE.md).
