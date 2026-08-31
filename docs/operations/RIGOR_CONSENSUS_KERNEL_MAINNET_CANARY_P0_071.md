# Consensus Kernel mainnet canary (P0-071)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-MAINNET-CANARY-ACTIVATION-P0-071`  
**Date:** 2026-08-31  
**Nature:** Compare-only on mainnet. No ConnectBlock switch.

```
CONSENSUS_KERNEL_MAINNET_CANARY = ACTIVE
CANARY_ROLE                    = VERIFY_ONLY
CPP_AUTHORITY                  = ACTIVE
PRODUCTION_KERNEL_SWITCHED     = FALSE
REFINEMENT_PROVED              = TRUE
EQUIVALENCE_PROVED             = FALSE
PRODUCTION_MAINNET_CANARY_ARMED = TRUE
```

Default remains **OFF**. The unique block decision is handwritten C++.

---

## Activation procedure

Both must be true:

1. Operator explicit: `-enableconsensuskernelcanary`
2. Release manifest: `PRODUCTION_MAINNET_CANARY_ARMED = TRUE`

Otherwise init fails with `MAINNET_CANARY_NOT_ARMED`.

This is compare-only. Candidate output never decides accept/reject.

---

## Monitoring

`MainnetCanaryMonitor`: `start` / `stop` / `status` / `disable_on_failure`

Metrics (optional telemetry, not a consensus log):

- `mainnet_canary_blocks_total`
- `mainnet_canary_compare_total`
- `mainnet_canary_discrepancy_total`
- `mainnet_canary_runtime_cpp`
- `mainnet_canary_runtime_candidate`

Phase-1 soak: 10000 ConnectBlock-shaped cases (normal / empty / high-tx / large UTXO / RES maturity / draw).

---

## Rollback

Mismatch token: `CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY`

Record: height, block hash, txid, cpp_result, candidate_result, authority_name.

Then:

1. Detect
2. Disable canary compare
3. Continue C++ production path

Operator rollback: omit the flag and restart.

No fallback candidate. No auto-repair.

Long-run observation (P0-072):
[`RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN_P0_072.md`](./RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN_P0_072.md).

---

## Incident response

1. Capture telemetry (if a log dir was set).
2. Confirm ConnectBlock still used C++ authority.
3. Leave canary disabled until the mismatch is understood.
4. Do not treat a finite soak as equivalence (`EQUIVALENCE_PROVED = FALSE`).

---

## No authority switch

- ConnectBlock acceptance logic is unchanged.
- C++ authorities are not deleted or replaced.
- Issuance / RES / draw / supply / PoW / ASERT are unchanged.
- `PRODUCTION_KERNEL_SWITCHED = FALSE`
