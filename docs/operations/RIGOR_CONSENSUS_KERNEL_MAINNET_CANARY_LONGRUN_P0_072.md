# Consensus Kernel mainnet canary long-run observation (P0-072)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-MAINNET-CANARY-LONG-RUN-P0-072`  
**Date:** 2026-08-31  
**Nature:** Long-run compare-only observation. Not a production migration.

```
CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN = PASS
BLOCKS_MONITORED                       >= 100000
DISCREPANCY_COUNT                      = 0
FAULT_INJECTION                        = PASS
ROLLBACK                               = PASS
CPP_AUTHORITY                          = ACTIVE
CANARY_ROLE                            = VERIFY_ONLY
CONSENSUS_KERNEL_CANDIDATE             = VERIFY_ONLY
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                      = TRUE
EQUIVALENCE_PROVED                     = FALSE
```

Default remains **OFF**. The unique block decision is handwritten C++.

---

## Activation state

| Item | Value |
|------|--------|
| Production decision | `CPP_AUTHORITY = ACTIVE` |
| Canary | `CONSENSUS_KERNEL_CANDIDATE = VERIFY_ONLY` |
| Production switch | `EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)` |
| Operator flag | `-enableconsensuskernelcanary` (default off) |
| Release arm | `PRODUCTION_MAINNET_CANARY_ARMED = FALSE` |
| Health default | `PAUSED` until `MainnetCanaryMonitor::Start()` |

Observation mode:

```
Block
  → ConnectBlock
  → C++ authority decision          ← unique accept/reject
parallel:
  Consensus Kernel Canary           ← compare only
```

Decision owner: **C++ only**.

---

## Observation period

Target: **100000** ConnectBlock-shaped mainnet cases (`MAINNET_CANARY_LONGRUN_BLOCKS`).

This is a deterministic dual-path soak of ConnectBlock-shaped pins (genesis,
draw, classify, spends, RES maturity, empty block, high-tx, large UTXO,
convergence draw). It is **not** 100000 mined live-chain blocks and it is
**not** an equivalence proof.

Mainnet canary is disarmed (`PRODUCTION_MAINNET_CANARY_ARMED = FALSE`).
No committed observation file is kept: a stale `mainnet_canary_observation.json`
would overclaim a soak that cannot run while the arm is false.

---

## Metrics

Collected by `MainnetCanaryMonitor`:

- `blocks_checked`
- `comparison_count`
- `cpp_execution_time` (`mainnet_canary_runtime_cpp`)
- `candidate_execution_time` (`mainnet_canary_runtime_candidate`)
- `rss_delta`
- `memory_growth`
- `discrepancy_count`

Expected: `DISCREPANCY_COUNT = 0`.

JSON export fields: `start_height`, `end_height`, `blocks_checked`,
`comparisons`, `discrepancies`, `rollback_events`, `runtime_statistics`.

---

## Health and incident handling

`ConsensusKernelCanaryHealth` states: `ACTIVE` / `PAUSED` / `DISABLED`.

Any discrepancy:

1. Emit `CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY`
2. Record height, block hash, txid (if available), authority name, cpp result, candidate result
3. Disable compare (`DISABLED`)
4. Continue the C++ consensus path

`CANARY_FAULT_INJECTION_TEST` is **false** in production. Tests may call
`FaultInject` for issuance / draw / classify / ledger-delta mismatches.
Detection, shutdown of compare, and C++ continuation must all pass. The
candidate is never accepted.

---

## Rollback procedure

1. Discrepancy latch disables compare. ConnectBlock is unchanged.
2. Operator rollback: omit `-enableconsensuskernelcanary` and restart.
3. No candidate fallback. No auto-repair.
4. Do not treat a finite soak as equivalence (`EQUIVALENCE_PROVED = FALSE`).

---

## Authority ownership

- ConnectBlock acceptance logic is unchanged.
- C++ authorities are not deleted or replaced.
- Issuance / RES / draw / supply / PoW / ASERT are unchanged.
- OCaml runtime is not linked.
- `EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)`
