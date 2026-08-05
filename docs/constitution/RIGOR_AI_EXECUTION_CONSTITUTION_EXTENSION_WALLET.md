# RIGOR AI Execution Constitution — Wallet Extension

**Task:** `TASK-WALLET-SIM-001`  
**Version:** 1.0  
**Date:** 2026-06-22  
**Type:** Documentation only — **no consensus / mining / protocol / wallet production changes**  
**Parent:** [RIGOR_EXECUTION_CONSTITUTION.md](./RIGOR_EXECUTION_CONSTITUTION.md)

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Mining Impact | **NONE** |
| Wallet Production Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

---

## §W-0 — Purpose

This extension defines the **User Execution Layer** as a governed simulation domain. The constitution here is no longer only a development rule set — it becomes a **user behavior consistency constraint system**.

It answers:

> Given identical user inputs and frozen UX rules, do wallet clients and GUI surfaces produce reproducible, convergent behavior?

```
equivalence_claimed = false
```

Wallet simulation PASS **does not** claim runtime equivalence with live nodes, production wallets, or mainnet conditions.

---

## §W-1 — User Execution Layer Definition

The **User Execution Layer (UEL)** is the bounded domain where human-facing wallet and GUI actions interact with **read-only authority views** of chain state.

| Component | Role | Authority |
|-----------|------|-----------|
| **Wallet client** | Displays ORU/RES balances, pending txs, send/cancel/retry UX | **Non-consensus** — presentation only |
| **GUI surface** | Renders wallet state snapshots | **Non-consensus** — replayable view |
| **Behavior model** | Deterministic user action schedule | **Simulation input** |
| **Authority feed** | Simulated RPC/explorer convergence data | **Read-only** in simulation |

**UEL boundaries:**

- UEL MUST NOT modify consensus, mining, or protocol rules.
- UEL MUST NOT implement production signing, key storage, or broadcast to live nodes.
- UEL MAY simulate user actions against deterministic virtual state.

---

## §W-2 — Wallet Deterministic State Rules

Every virtual wallet client MUST maintain a **canonical deterministic state** derivable from action history and authority feed ticks.

### W-DS-01 — State components

| Field | Type | Rule |
|-------|------|------|
| `oru_confirmed` | integer (satoshi-scale units) | Decreases on confirmed send; increases on receive |
| `oru_pending` | integer | Sum of in-flight outbound ORU |
| `res_confirmed` | integer | Updated only after convergence lag elapses |
| `res_pending` | integer | Mint-eligible RES not yet visible in UX |
| `pending_txs` | ordered list | Sorted by `(created_tick, txid)` |
| `gui_view_tick` | integer | Last rendered tick |

### W-DS-02 — State hash

```
wallet_state_hash = SHA256(canonical_json({
  oru_confirmed, oru_pending,
  res_confirmed, res_pending,
  pending_txs: sorted by txid,
  gui_view_tick
}))
```

Identical action sequences and authority feeds MUST yield identical `wallet_state_hash`.

### W-DS-03 — No hidden UX state

Correctness-relevant wallet simulation state MUST NOT depend on wall-clock time, random IDs, or unseeded ordering.

---

## §W-3 — ORU → RES UX Deterministic Mapping

RES visibility in the User Execution Layer follows a **lagged, authority-driven mapping** — not local inference.

| Stage | ORU event | RES UX effect | Lag (ticks) |
|-------|-----------|---------------|-------------|
| Send ORU | Outbound tx created | None | 0 |
| Confirm ORU | Tx confirmed on chain view | None | 0 |
| Convergence eligible | Authority feed marks draw window | `res_pending` may increase | +`convergence_lag` |
| RES mint visible | Authority feed emits mint record | `res_confirmed` increases, `res_pending` decreases | +`res_perception_lag` |

### W-RES-01 — Mapping rules

1. ORU and RES balances MUST remain **display-separated** (no combined totals).
2. RES fields MUST include linkage context: `mint_txid`, `convergence_root`, `bind_height` when visible.
3. `res_perception_lag` is a simulation parameter modeling UX delay — not a protocol rule.
4. Mapping is **deterministic**: same authority feed tick sequence ⇒ same RES display state.

### W-RES-02 — Forbidden UX

- Token-style interchangeable balance badges
- Local convergence eligibility computation in simulation claiming protocol authority
- Combining ORU + RES into a single "total balance"

---

## §W-4 — Multi-Client Consistency Rule

When N wallet clients observe the same authority feed and perform scheduled actions:

| Rule | Definition |
|------|------------|
| **MC-01** | After reconciliation tick, all clients MUST share identical `wallet_state_hash` per wallet ID |
| **MC-02** | Cross-client divergence during stress is permitted; unresolved divergence at end is FAIL |
| **MC-03** | Reconciliation uses deterministic merge: max confirmed balances from authority, sorted pending tx union |
| **MC-04** | Client-specific GUI lag MAY differ; post-reconciliation canonical state MUST converge |

Multi-client consistency validates **convergence of simulated wallet views**, not live node equivalence.

---

## §W-5 — GUI Replay Equivalence Rule

**Important:** This is **behavior consistency**, not system equivalence.

| Rule | Meaning |
|------|---------|
| **GUI-R-01** | Given identical GUI input trace, replay MUST produce identical `gui_state_hash` |
| **GUI-R-02** | GUI replay validates **rendered view determinism** — not consensus replay |
| **GUI-R-03** | `gui_state_hash` covers: displayed ORU, displayed RES, pending list, status labels |
| **GUI-R-04** | GUI replay PASS + `equivalence_claimed = false` is the only valid combination |

```
gui_behavior_consistent = (replay_hash_a == replay_hash_b)
equivalence_claimed = false   // always
```

GUI replay equivalence MUST NOT be interpreted as PROOF-034 or SIM-011 equivalence.

---

## §W-6 — Simulation Domain Placement

| Layer | Wallet role |
|-------|-------------|
| PROOF-034 | Runtime archive truth — wallet sim reads nothing from live archive |
| SIM-011 | System replay — no RPC bridge to wallet sim |
| SIM-CLUSTER | Network stress — complementary; wallet sim is client-side |
| **WALLET-SIM-001** | **User Execution Layer validation (this extension)** |

---

## §W-7 — Hard Constraints (Binding)

| Constraint | Status |
|------------|--------|
| NO consensus modification | **CONFIRMED** |
| NO mining changes | **CONFIRMED** |
| NO protocol changes | **CONFIRMED** |
| NO wallet production implementation | **CONFIRMED** |
| ONLY deterministic simulation | **CONFIRMED** |
| `equivalence_claimed = true` | **FORBIDDEN** |

---

## §W-8 — Violation Classes

| Class | Description | Severity |
|-------|-------------|----------|
| **UEL-V1** | Wallet sim writes to consensus paths | CRITICAL |
| **UEL-V2** | Production wallet code modified for sim | CRITICAL |
| **UEL-V3** | GUI replay claims system equivalence | CRITICAL |
| **UEL-V4** | Non-deterministic wallet state hash | HIGH |
| **UEL-V5** | ORU/RES display merge in simulation | MEDIUM |

---

## §W-9 — Output Table

| Field | Status |
|-------|--------|
| User Execution Layer defined | **COMPLETE** |
| Wallet deterministic state rules | **COMPLETE** |
| ORU → RES UX mapping | **COMPLETE** |
| Multi-client consistency rule | **COMPLETE** |
| GUI replay equivalence rule | **COMPLETE** |
| Protocol Impact | **NONE** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR AI Execution Constitution — Wallet Extension — TASK-WALLET-SIM-001*
