# PROOF-002: Golden RES Consensus Vectors

**Task:** `TASK-PROOF-003`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (framework); vector entries **DRAFT**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md), SPEC-002, SPEC-005, SPEC-006  
**Category:** GV-003 (RES Mint Transition)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document populates the initial **RES Golden Consensus Vectors**
`GV-RES-0001` … `GV-RES-0003` with canonical expected values for comparison
across:

```
Specification (SPEC-002 RES recurrence)
        ↓
Formal Model (RFC-015 S_RES equation)
        ↓
Coq Proof Model (ReplayState / ConnectBlockMint)
        ↓
Bitcoin-v25 Runtime (m_chain_res_mint_total / FVR mirror)
```

This task is **proof infrastructure only**. No consensus, validation, state_hash,
FVR, or monetary-policy changes.

**Equivalence Claimed:** false

---

## 1. Purpose

Establish normative RES mint vector mappings so every verification layer can
compare identical inputs against declared outputs.

| Layer | RES obligation |
|-------|----------------|
| **Specification** | `S_RES(0)=0`; `S_RES(h+1) = S_RES(h) + RES_REWARD_AMOUNT · M(h+1)` ([SPEC-002](../../specification/SPEC-002-ECONOMIC-MODEL.md) §5.1) |
| **Formal model** | `M(h) ∈ {0,1}`; cap `S_RES(h) ≤ RES_MAX_SUPPLY`; mint amount fixed at `RES_REWARD_AMOUNT` |
| **Coq** | `rs_res_supply`, `cb_res_mint_delta`, `res_mint_count_M_h` (`ConnectBlockMint.v`) |
| **Runtime** | `m_chain_res_mint_total`, `m_block_res_mint_sum`; FVR-B/C/E mirrors |

Vectors are **normative fixtures** ([PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) §4).
Draw internals are declared as inputs (`draw_won`, `|F| ≥ 1`); draw equivalence
is **not** claimed (FVR-M).

---

## 2. Frozen RES Monetary Parameters

Locked in `rigor_consensus_lock.h` / `res_amount.h`:

| Symbol | Display | Atoms |
|--------|---------|-------|
| `RES_ATOMIC_UNIT` | `0.01 RES` | `1` |
| `RES_COIN` | `1.00 RES` | `100` |
| `RES_REWARD_AMOUNT` | `100.00 RES` | `10_000` |
| `RES_MAX_SUPPLY` | `21_000_000 RES` | `2_100_000_000` |

**Recurrence (RFC-015 / SPEC-002):**

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + RES_REWARD_AMOUNT · M(h+1)    where M(h+1) ∈ {0,1}
```

**Cap rule:** A mint is valid only if
`S_RES(h−1) + RES_REWARD_AMOUNT ≤ RES_MAX_SUPPLY`.

**Mint amount rule:** When `M(h)=1`, credited amount MUST equal `RES_REWARD_AMOUNT`
exactly (CI-024 / RES-L3 / RES-L6).

---

## 3. Vector Registry Entries

### GV-RES-0001 — Genesis RES Mint & Initial `S_RES`

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-RES-0001` |
| **Category** | GV-003 |
| **Status** | **DRAFT** |
| **Description** | First successful RES draw mint on chain: parent supply zero → initial non-zero `S_RES` |
| **Spec refs** | SPEC-002 §5.1, §5.4; SPEC-005 CI-022, CI-024; CR-043..CR-045 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `10` |
| `parent_tip` | Valid block at height `9` with `S_RES(9) = 0` |
| `draw_won` | `true` (convergence draw selects exactly one RES mint candidate) |
| `draw_gate` | `|F| ≥ 1` (`CONVERGENCE_DRAW_MIN_TXS_FOR_RES`) |
| `converge_present` | Yes — valid Converge txs in block (post-converge mint path) |
| `M(h)` | `1` |
| `valid_block` | Yes |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `9` |
| `S_RES(9)` | `0` atoms (`0.00 RES`) |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `10` |
| `S_RES(10)` | `10_000` atoms (`100.00 RES`) |

#### Expected RES Mint Delta

| Field | Atoms | RES |
|-------|-------|-----|
| `M(10)` | — | `1` |
| `res_mint_block` | `10_000` | `100.00` |
| `Δ_RES` | `+10_000` | `+100.00` |

Recurrence check: `0 + 10_000 × 1 = 10_000` atoms.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **RES-L1** | `res_mint_cumulative ≥ 0` | **PASS** |
| **RES-L2** | `res_mint_cumulative ≤ res_max_supply` | **PASS** |
| **RES-L3** | `res_mint_block ∈ {0, RES_REWARD_AMOUNT}` | **PASS** (`10_000`) |
| **RES-L4** | `res_mint_count_block ∈ {0, 1}` | **PASS** (`1`) |
| **RES-L5** | `S_RES(h) = S_RES(h−1) + res_mint_block` | **PASS** |
| **RES-L6** | `res_mint_block = RES_REWARD_AMOUNT ⟺ M(h)=1` | **PASS** |
| **RES-L7** | Cap headroom: `S_RES(h−1) + RES_REWARD_AMOUNT ≤ RES_MAX_SUPPLY` | **PASS** |
| **CI-022** | RES recurrence preservation | **PASS** |
| **CI-024** | Single RES mint (`M(h) ∈ {0,1}`) | **PASS** |

> **Note on RES-L7:** FVR-E defines RES-L1..L6. **RES-L7** is a proof-layer
> label for cap headroom (CI-023 precondition for mint acceptance). CI-023
> full cap bound is exercised in GV-RES-0003.

#### Expected Runtime Witness Values (FVR-B / FVR-C / FVR-F)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `10` |
| `res_mint_parent` | `rs_res_supply_parent` | `0` |
| `res_mint_cumulative` | `rs_res_supply` | `10000` |
| `res_mint_block` | `cb_res_mint_delta` | `10000` |
| `res_mint_count_block` | `res_mint_count_M_h` | `1` |
| `res_max_supply` | (constant) | `2100000000` |
| `res_reward_amount` | (constant) | `10000` |

FVR-E: `passed = true`, `checked_invariants = 10`, `violations = []`.

#### Expected Coq Values

| Field | Expected |
|-------|----------|
| `rs_res_supply_parent` | `0` |
| `rs_res_supply` | `10000` |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |
| `cm_draw_won` | `true` |

Coq `apply_connectblock_supply_delta` matches runtime when `cm_draw_won = true`
and cap allows (`rs_res_supply + 10000 ≤ 2100000000`).

#### Expected Acceptance Result

**ACCEPT** — valid block; first chain RES mint; draw won; single mint at
`RES_REWARD_AMOUNT`.

---

### GV-RES-0002 — Single-Mint Post-Converge Block

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-RES-0002` |
| **Category** | GV-003 |
| **Status** | **DRAFT** |
| **Description** | Mid-chain block with Converge activity and exactly one RES mint |
| **Spec refs** | SPEC-002 §5.1..§5.4; SPEC-005 CI-022, CI-024; CR-043, CR-052 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `500` |
| `parent_tip` | Height `499` with `S_RES(499) = 10_000` atoms (one prior mint) |
| `draw_won` | `true` |
| `converge_present` | Yes — non-zero ORU converge burn in block (independent of RES step) |
| `converge_burn_C(h)` | `50 ORU` (fixture; does not affect RES recurrence) |
| `M(h)` | `1` |
| `valid_block` | Yes |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `499` |
| `S_RES(499)` | `10_000` atoms (`100.00 RES`) |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `500` |
| `S_RES(500)` | `20_000` atoms (`200.00 RES`) |

#### Expected RES Mint Delta

| Field | Atoms | RES |
|-------|-------|-----|
| `M(500)` | — | `1` |
| `res_mint_block` | `10_000` | `100.00` |
| `Δ_RES` | `+10_000` | `+100.00` |

Recurrence check: `10_000 + 10_000 × 1 = 20_000` atoms.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **RES-L1** | `res_mint_cumulative ≥ 0` | **PASS** |
| **RES-L2** | `res_mint_cumulative ≤ res_max_supply` | **PASS** |
| **RES-L3** | `res_mint_block ∈ {0, RES_REWARD_AMOUNT}` | **PASS** |
| **RES-L4** | `res_mint_count_block ∈ {0, 1}` | **PASS** (`1`) |
| **RES-L5** | `S_RES(h) = S_RES(h−1) + res_mint_block` | **PASS** |
| **RES-L6** | mint amount present iff `M(h)=1` | **PASS** |
| **RES-L7** | Cap headroom before mint | **PASS** |
| **CI-022** | RES recurrence preservation | **PASS** |
| **CI-024** | Single RES mint | **PASS** |

#### Expected Runtime Witness Values (FVR-B / FVR-C / FVR-F)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `500` |
| `res_mint_parent` | `rs_res_supply_parent` | `10000` |
| `res_mint_cumulative` | `rs_res_supply` | `20000` |
| `res_mint_block` | `cb_res_mint_delta` | `10000` |
| `res_mint_count_block` | `res_mint_count_M_h` | `1` |
| `res_max_supply` | (constant) | `2100000000` |
| `res_reward_amount` | (constant) | `10000` |

FVR-E: `passed = true`, `violations = []`.

Witness chain (FVR-F): child `rs_res_supply = 20000` links to parent witness
`rs_res_supply = 10000` at height `499`.

#### Expected Coq Values

| Field | Expected |
|-------|----------|
| `rs_res_supply_parent` | `10000` |
| `rs_res_supply` | `20000` |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |
| `cm_draw_won` | `true` |

#### Expected Acceptance Result

**ACCEPT** — post-converge block with exactly one RES mint at `RES_REWARD_AMOUNT`.

---

### GV-RES-0003 — Cap-Approaching Mint (`RES_MAX_SUPPLY` Boundary)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-RES-0003` |
| **Category** | GV-003 |
| **Status** | **DRAFT** |
| **Description** | Final admissible mint: parent one reward below cap → terminal `S_RES = RES_MAX_SUPPLY` |
| **Spec refs** | SPEC-002 §5.3; SPEC-005 CI-022, CI-023, CI-024; CR-045 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `2_100_000` (fixture height; large but valid) |
| `parent_tip` | Height `2_099_999` with `S_RES = RES_MAX_SUPPLY − RES_REWARD_AMOUNT` |
| `draw_won` | `true` |
| `M(h)` | `1` |
| `valid_block` | Yes |

**Cap boundary arithmetic:**

```
S_RES(h−1) = 2_100_000_000 − 10_000 = 2_099_990_000 atoms  (20_999_900.00 RES)
S_RES(h)   = 2_100_000_000 atoms                          (21_000_000.00 RES)
```

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `2_099_999` |
| `S_RES(2_099_999)` | `2_099_990_000` atoms (`20_999_900.00 RES`) |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `2_100_000` |
| `S_RES(2_100_000)` | `2_100_000_000` atoms (`21_000_000.00 RES`) — **at cap** |

#### Expected RES Mint Delta

| Field | Atoms | RES |
|-------|-------|-----|
| `M(2_100_000)` | — | `1` |
| `res_mint_block` | `10_000` | `100.00` |
| `Δ_RES` | `+10_000` | `+100.00` |

Recurrence check: `2_099_990_000 + 10_000 = 2_100_000_000 = RES_MAX_SUPPLY`.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **RES-L1** | `res_mint_cumulative ≥ 0` | **PASS** |
| **RES-L2** | `res_mint_cumulative ≤ res_max_supply` | **PASS** (equality at cap) |
| **RES-L3** | `res_mint_block ∈ {0, RES_REWARD_AMOUNT}` | **PASS** |
| **RES-L4** | `res_mint_count_block ∈ {0, 1}` | **PASS** (`1`) |
| **RES-L5** | `S_RES(h) = S_RES(h−1) + res_mint_block` | **PASS** |
| **RES-L6** | mint amount present iff `M(h)=1` | **PASS** |
| **RES-L7** | `S_RES(h) = RES_MAX_SUPPLY` after mint; prior headroom exactly one reward | **PASS** |
| **CI-022** | RES recurrence preservation | **PASS** |
| **CI-023** | `S_RES(h) ≤ RES_MAX_SUPPLY` | **PASS** (tight: equals cap) |
| **CI-024** | Single RES mint | **PASS** |

**Follow-on rule (informative, not this vector's block):** Any block at height
`> 2_100_000` with `S_RES(h−1) = RES_MAX_SUPPLY` MUST have `M(h)=0` and be
**REJECT** if a positive mint is attempted (CR-045). That rejection scenario
is reserved for a future GV-RES-0004+ entry.

#### Expected Runtime Witness Values (FVR-B / FVR-C / FVR-F)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `2100000` |
| `res_mint_parent` | `rs_res_supply_parent` | `2099990000` |
| `res_mint_cumulative` | `rs_res_supply` | `2100000000` |
| `res_mint_block` | `cb_res_mint_delta` | `10000` |
| `res_mint_count_block` | `res_mint_count_M_h` | `1` |
| `res_max_supply` | (constant) | `2100000000` |
| `res_reward_amount` | (constant) | `10000` |

FVR-E: `passed = true`, `violations = []`.

#### Expected Coq Values

| Field | Expected |
|-------|----------|
| `rs_res_supply_parent` | `2099990000` |
| `rs_res_supply` | `2100000000` |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |
| `cm_draw_won` | `true` |

Coq cap guard: `rs_res_supply + cb_RES_MINT_AMOUNT ≤ cb_MAX_RES_SUPPLY` holds
with equality on the after-state.

#### Expected Acceptance Result

**ACCEPT** — final mint fills remaining cap headroom exactly; cumulative supply
reaches `RES_MAX_SUPPLY`.

---

## 4. Vector Authority

Per [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) §4:

| Rule | Application |
|------|-------------|
| **Normative** | When `ESTABLISHED`, all layers MUST match declared RES outputs on declared inputs. |
| **Runtime** | `m_chain_res_mint_total`, `m_block_res_mint_sum`, FVR mirrors MUST conform. Witnesses are evidence only. |
| **Coq** | `apply_connectblock_supply_delta` MUST match `rs_res_supply` step when `cm_draw_won` and cap guard align. |
| **No back-flow** | Runtime exports MUST NOT amend vector expected values. |
| **Draw boundary** | `draw_won` is a declared input; draw equivalence is not claimed (FVR-M). |

---

## 5. Status Lifecycle

| Vector | PROOF-000 (prior) | This task | Next gate |
|--------|-------------------|-----------|-----------|
| GV-RES-0001 | RESERVED | **DRAFT** | Phase F runtime harness + review |
| GV-RES-0002 | RESERVED | **DRAFT** | Phase F + FVR-F witness chain check |
| GV-RES-0003 | RESERVED | **DRAFT** | Phase F cap-boundary harness |

**Promotion criteria (DRAFT → ESTABLISHED):**

1. Values trace to SPEC-002 §5 and frozen `RES_REWARD_AMOUNT` / `RES_MAX_SUPPLY`.
2. Runtime mirror on conforming fixture blocks matches all witness fields.
3. FVR-E reports `passed = true`.
4. Coq `cb_res_mint_delta` / `rs_res_supply` match on draw-won, cap-allowed paths.
5. No specification amendment required.

---

## 6. Relationships

### SPEC-002 — RES recurrence

All vectors instantiate:

\[
S_{\text{RES}}(h+1) = S_{\text{RES}}(h) + \texttt{RES\_REWARD\_AMOUNT} \cdot M(h+1)
\]

with `M(h) ∈ {0,1}` and fixed mint amount `RES_REWARD_AMOUNT`.

### SPEC-005 — Economic invariants

| Invariant | Vectors |
|-----------|---------|
| **CI-022** (recurrence) | All three |
| **CI-023** (cap) | GV-RES-0003 primary; GV-RES-0001/0002 via headroom |
| **CI-024** (single mint) | All three (`M(h)=1` with count `1`) |

### SPEC-006 — Layer mapping

| Vector field | Coq | Runtime (FVR) |
|--------------|-----|---------------|
| `S_RES` | `rs_res_supply` | `res_mint_cumulative` |
| `M(h)·reward` | `cb_res_mint_delta` | `res_mint_block` |
| `M(h)` | `res_mint_count_M_h` | `res_mint_count_block` |
| Recurrence | `res_supply_equation_theorems` | RES-L5 |
| Cap | `res_cap_theorems` | RES-L2 |

### FVR-C / FVR-F — Mirror & witness chain

| Layer | Role |
|-------|------|
| **FVR-C** | Coq field hook: `rs_res_supply`, `cb_res_mint_delta`, `res_mint_count_M_h` |
| **FVR-F** | Per-height witness chain; parent→child `rs_res_supply` linkage (GV-RES-0002) |
| **FVR-E** | RES-L1..L6 active verification |

Witness match is corroboration only; `equivalence_claimed = false`.

---

## 7. Freeze Compatibility

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No validation modifications | **Confirmed** |
| No state_hash modifications | **Confirmed** |
| No runtime witness (FVR) modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## Task Output Status

| Field | Status |
|-------|--------|
| RES Golden Vector Framework | **ESTABLISHED** |
| Vector Entries GV-RES | **DRAFT** |
| Proof Infrastructure | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-003` — GV-RES-0001..0003 populated (DRAFT); frozen RES constants documented |
