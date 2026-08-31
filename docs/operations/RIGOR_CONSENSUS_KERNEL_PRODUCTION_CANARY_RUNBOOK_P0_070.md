# Consensus Kernel production canary runbook (P0-070)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PRODUCTION-CANARY-READINESS-P0-070`  
**Date:** 2026-08-31  
**Nature:** Controlled rollout boundary, rollback, release safety.

```
CONSENSUS_KERNEL_PRODUCTION_CANARY_READY = PASS
CANARY_AVAILABLE                         = TRUE
CANARY_ENABLED                           = FALSE
ROLLBACK_READY                           = TRUE
PRODUCTION_KERNEL_SWITCHED               = FALSE
REFINEMENT_PROVED                        = TRUE
EQUIVALENCE_PROVED                       = FALSE
PRODUCTION_MAINNET_CANARY_ARMED          = TRUE
```

Superseded for activation by
[`RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_P0_071.md`](./RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_P0_071.md).
Default is still **OFF**. Compare-only; ConnectBlock is not switched.

---

## What the canary is

`-enableconsensuskernelcanary` is compare-only:

```
Block received
    → ConnectBlock
    → handwritten C++ authority     ← production accept/reject
    → optional candidate compare    ← observe only
```

Default: **OFF**. Mainnet compare-only requires `PRODUCTION_MAINNET_CANARY_ARMED`
and `-enableconsensuskernelcanary`. See P0-071.

Only an explicit operator action enables compare. ConnectBlock is never switched.

---

## Enable procedure

1. Confirm gates: `python3 tools/check-production-canary-readiness.py`
2. For mainnet compare-only, follow P0-071. For testnet/regtest, `-enableconsensuskernelcanary` is enough.
3. Confirm the flag is not a consensus switch.
4. Optional telemetry directory: `logs/consensus_kernel_canary/` (off unless set).

---

## Disable procedure

Omit the flag and restart.

Runtime latch: `ConsensusKernelCanaryController::Disable` stops further
compare immediately. ConnectBlock continues on C++ authority.

---

## Rollback

| Event | Action |
|---|---|
| Operator rollback | Restart without the flag |
| Discrepancy | Log incident, disable compare, keep C++ authority |
| Crash in compare | Disable compare, keep C++ authority |

Rollback **never** falls back to the candidate and **never** auto-repairs.

---

## Incident handling

Token: `CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY`

Record:

- height
- block hash
- txid
- cpp result
- candidate result

Then:

1. Record (optional telemetry only; not a consensus log)
2. Disable canary compare
3. Continue production C++ authority

Do not treat a finite compare as equivalence. `EQUIVALENCE_PROVED = FALSE`.

---

## No consensus switch

- ConnectBlock decision remains handwritten C++.
- Authorities are not replaced or deleted.
- Extracted OCaml is not linked.
- Issuance / RES / draw / supply / PoW / ASERT are unchanged.
- `PRODUCTION_KERNEL_SWITCHED = FALSE`
