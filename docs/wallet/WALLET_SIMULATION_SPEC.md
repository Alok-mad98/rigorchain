# Wallet Simulation Specification

**Task:** `TASK-WALLET-SIM-001`  
**Version:** 1.0  
**Date:** 2026-06-22  
**Type:** Documentation + deterministic simulation — **no consensus / mining / protocol / wallet production changes**

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Wallet Production Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

**Constitution:** [RIGOR_AI_EXECUTION_CONSTITUTION_EXTENSION_WALLET.md](../constitution/RIGOR_AI_EXECUTION_CONSTITUTION_EXTENSION_WALLET.md)

---

## 0. Purpose

Validate the **User Execution Layer** before real multi-client wallet deployment:

- GUI behavior reproducibility
- Wallet state determinism
- ORU → RES UX consistency
- Multi-client convergence under stress

```
equivalence_claimed = false
```

Simulation results do not imply runtime equivalence under real network or production wallet conditions.

---

## 1. Architecture

```text
┌─────────────────────────────────────────────────────────────────┐
│  gui_state_replay_validator.py  →  gui_replay_report.json       │
├─────────────────────────────────────────────────────────────────┤
│  multi_client_wallet_engine.py  →  wallet_convergence_report    │
├─────────────────────────────────────────────────────────────────┤
│  user_behavior_model.json  +  wallet_stress_scenarios.json      │
├─────────────────────────────────────────────────────────────────┤
│  Virtual authority feed (convergence, confirm, mint events)     │
│  10 wallet clients · GUI lag · RES perception lag               │
└─────────────────────────────────────────────────────────────────┘
         │
         ▼
  .runtime/wallet-sim/
    wallet_state_drift.jsonl
    gui_replay_trace.jsonl
    multi_client_convergence.json
    stress_scenario_results.json
```

---

## 2. Simulated User Behaviors

Defined in [`sim/wallet_sim/user_behavior_model.json`](../../sim/wallet_sim/user_behavior_model.json):

| Action | Description |
|--------|-------------|
| `send_oru` | Create outbound ORU transfer |
| `cancel_tx` | Cancel pending outbound tx |
| `retry_tx` | Re-submit cancelled or failed tx |
| `delayed_confirm` | Authority confirms tx after N ticks |
| `res_perception_lag` | RES mint visible after lag ticks |

---

## 3. Multi-Client Engine

[`sim/wallet_sim/multi_client_wallet_engine.py`](../../sim/wallet_sim/multi_client_wallet_engine.py):

| Feature | Model |
|---------|--------|
| N wallet clients | Independent state + shared authority feed |
| ORU send/cancel/retry | Deterministic tx lifecycle |
| RES visibility | Lagged mapping from authority mint events |
| GUI lag | Per-client render delay (ticks) |
| Reconciliation | Deterministic merge at scenario end |

---

## 4. GUI State Replay

[`sim/wallet_sim/gui_state_replay_validator.py`](../../sim/wallet_sim/gui_state_replay_validator.py):

- Records `gui_state_hash` per tick per client
- Replays trace twice; compares hashes
- Validates **behavior consistency** (not system equivalence)

---

## 5. Stress Scenarios

[`sim/wallet_sim/scenarios/wallet_stress_scenarios.json`](../../sim/wallet_sim/scenarios/wallet_stress_scenarios.json):

| Scenario ID | Description |
|-------------|-------------|
| `concurrent_send_oru` | 10 wallets send ORU simultaneously |
| `delayed_convergence` | Convergence authority delayed 8 ticks |
| `double_click_send` | Duplicate send action same tick |
| `fork_during_gui_view` | Divergent chain view during GUI render |
| `stale_balance_display` | GUI shows stale balance until refresh tick |

---

## 6. Metrics Output

Directory: **`.runtime/wallet-sim/`**

| File | Content |
|------|---------|
| `wallet_state_drift.jsonl` | Per-tick per-client wallet + GUI hashes |
| `gui_replay_trace.jsonl` | GUI snapshot timeline |
| `multi_client_convergence.json` | Cross-client hash match verdict |
| `stress_scenario_results.json` | Per-scenario pass/fail |
| `wallet_sim_report.json` | Full run summary |

---

## 7. Operations

```bash
cd .
python3 -m sim.wallet_sim.run_stress_test
```

Optional:

```bash
python3 -m sim.wallet_sim.run_stress_test \
  --runtime .runtime/wallet-sim \
  --seed 0x57414C4C455431 \
  --scenario concurrent_send_oru
```

Exit code `0` = all scenarios pass; `1` = failure.

---

## 8. Constraints (Binding)

| Constraint | Status |
|------------|--------|
| NO consensus modification | **CONFIRMED** |
| NO mining changes | **CONFIRMED** |
| NO protocol changes | **CONFIRMED** |
| NO wallet production implementation | **CONFIRMED** |
| ONLY deterministic simulation | **CONFIRMED** |
| `equivalence_claimed = true` | **FORBIDDEN** |

---

## 9. File Index

| Path | Purpose |
|------|---------|
| `docs/constitution/RIGOR_AI_EXECUTION_CONSTITUTION_EXTENSION_WALLET.md` | Constitution extension |
| `docs/wallet/WALLET_SIMULATION_SPEC.md` | This document |
| `sim/wallet_sim/multi_client_wallet_engine.py` | Multi-client engine |
| `sim/wallet_sim/user_behavior_model.json` | User action model |
| `sim/wallet_sim/gui_state_replay_validator.py` | GUI replay validator |
| `sim/wallet_sim/scenarios/wallet_stress_scenarios.json` | Stress scenarios |
| `sim/wallet_sim/run_stress_test.py` | Runner CLI |

---

## 10. Output Table

| Field | Status |
|-------|--------|
| Wallet Simulation Layer | **COMPLETE** |
| Multi-client Execution Model | **COMPLETE** |
| User Behavior Determinism | **COMPLETE** |
| GUI Replay Validation | **COMPLETE** |
| Constitution Extension | **COMPLETE** |
| Protocol Impact | **NONE** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of WALLET_SIMULATION_SPEC — TASK-WALLET-SIM-001*
