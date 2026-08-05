# PROOF-001: Golden ORU Consensus Vectors

**Task:** `TASK-PROOF-002`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (framework); vector entries **DRAFT**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md), SPEC-002, SPEC-005, SPEC-006  
**Category:** GV-002 (ORU Supply Transition)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document populates the initial **ORU Golden Consensus Vectors**
`GV-ORU-0001` … `GV-ORU-0003` with canonical expected values for comparison
across:

```
Specification (SPEC-002 recurrence)
        ↓
Formal Model (RFC-002 S_ORU equation)
        ↓
Coq Proof Model (ReplayState / ConnectBlockMint fields)
        ↓
Bitcoin-v25 Runtime (GetBlockSubsidy + m_chain_oru_total / FVR mirror)
```

This task is **proof infrastructure only**. No consensus, validation, state_hash,
FVR, or monetary-policy changes.

**Equivalence Claimed:** false

---

## 1. Purpose

Establish normative ORU supply vector mappings so every verification layer can
compare identical inputs against declared outputs.

| Layer | ORU obligation |
|-------|----------------|
| **Specification** | `S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)` ([SPEC-002](../../specification/SPEC-002-ECONOMIC-MODEL.md) §4.1) |
| **Formal model** | RFC-002 state equation; `R(h) = GetBlockSubsidy(h)` under frozen schedule |
| **Coq** | `rs_oru_supply`, `cb_oru_subsidy`; burn via `cb_oru_burn_delta` (runtime hook; Coq replay scaffold gap documented) |
| **Runtime** | `m_chain_oru_total`, FVR-B mirror fields; FVR-E ORU-L1..L4 checks |

Vectors are **normative fixtures** (PROOF-000 §4). They do not prove global
Coq ≡ Runtime equivalence.

---

## 2. Frozen ORU Subsidy Parameters

All three vectors use the locked mainnet/public-network schedule
(`rigor_consensus_lock.h`, `kernel/chainparams.cpp`):

| Symbol | Value |
|--------|-------|
| `ORU_INITIAL_BLOCK_SUBSIDY` | `1000 ORU` (= `100_000_000_000` atoms) |
| `ORU_SUBSIDY_HALVING_INTERVAL` | `210_000` blocks |
| `ORU_SUBSIDY_MAX_SHIFTS` | `3` |
| `ORU_MIN_BLOCK_SUBSIDY` | `125 ORU` (= `12_500_000_000` atoms) |

**Subsidy function** (`GetBlockSubsidy`):

```
R(h) = ORU_INITIAL_BLOCK_SUBSIDY >> min(⌊h / 210_000⌋, 3)
```

| Height range | `R(h)` (ORU) | Atoms |
|--------------|--------------|-------|
| `0 … 209_999` | 1000 | `100_000_000_000` |
| `210_000 … 419_999` | 500 | `50_000_000_000` |
| `420_000 … 629_999` | 250 | `25_000_000_000` |
| `≥ 630_000` | 125 (floor) | `12_500_000_000` |

**Atom scale:** `1 ORU = COIN = 100_000_000` base units (Tharn).

---

## 3. Vector Registry Entries

### GV-ORU-0001 — Genesis Subsidy & Initial `S_ORU`

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-ORU-0001` |
| **Category** | GV-002 |
| **Status** | **DRAFT** |
| **Description** | First chain block (genesis, height 0): initial subsidy credit with zero converge burn |
| **Spec refs** | SPEC-002 §4.1, §4.4; SPEC-005 CI-020, CI-021; CR-050 (coinbase subsidy) |

#### Input

| Field | Value |
|-------|-------|
| `height` | `0` |
| `block_role` | Genesis (chain origin) |
| `parent_tip` | `null` (no predecessor) |
| `converge_burn_sum_C(h)` | `0` |
| `valid_block` | Yes — well-formed genesis per frozen chainparams |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | — (pre-chain) |
| `S_ORU` | `0` atoms (`0 ORU`) — chain-origin running total before genesis connect |
| `S_RES` | `0` (unchanged; not ORU vector focus) |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `0` |
| `S_ORU(0)` | `100_000_000_000` atoms (`1000 ORU`) |
| `S_RES(0)` | `0` |

#### Expected ORU Subsidy & Burn Deltas

| Field | Atoms | ORU |
|-------|-------|-----|
| `R(0)` (`oru_subsidy_block`) | `100_000_000_000` | `1000` |
| `C(0)` (`oru_converge_burn_block`) | `0` | `0` |
| `Δ_ORU` | `+100_000_000_000` | `+1000` |

Recurrence check: `0 + 1000 − 0 = 1000 ORU`.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **ORU-L1** | `oru_supply_cumulative ≥ 0` | **PASS** |
| **ORU-L2** | `oru_converge_burn_block ≥ 0` | **PASS** |
| **ORU-L3** | `S_ORU(h) = S_ORU(h−1) + R(h) − C(h)` | **PASS** |
| **ORU-L4** | `oru_subsidy_block = GetBlockSubsidy(height)` | **PASS** |
| **CI-020** | `S_ORU(h) ≥ 0` | **PASS** |
| **CI-021** | ORU recurrence preservation | **PASS** |

> **Note on ORU-L5..L6:** FVR-E defines four ORU runtime labels (ORU-L1..L4).
> RES supply uses RES-L1..L6. This vector lists all applicable ORU labels plus
> authoritative CI-020/CI-021 from SPEC-005.

#### Expected Runtime Witness Values (FVR-B / FVR-C)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `0` |
| `oru_supply_parent` | `rs_oru_supply_parent` | `0` |
| `oru_supply_cumulative` | `rs_oru_supply` | `100000000000` |
| `oru_subsidy_block` | `cb_oru_subsidy` | `100000000000` |
| `oru_converge_burn_block` | `cb_oru_burn_delta` | `0` |
| `res_mint_block` | `cb_res_mint_delta` | `0` |
| `res_mint_count_block` | `res_mint_count_M_h` | `0` |

FVR-E: `passed = true`, `checked_invariants = 10`, `violations = []`.

#### Expected Coq Values

| Field | Expected | Scaffold note |
|-------|----------|---------------|
| `rs_oru_supply_parent` | `0` | — |
| `rs_oru_supply` | `100000000000` | Matches runtime |
| `cb_oru_subsidy` | `100000000000` | `cm_oru_subsidy` in mint spec |
| `cb_oru_burn_delta` | `0` | Runtime hook; not in `cb_oru_mint_delta` |
| `cb_oru_mint_delta` (scaffold) | `100000000000` | Equals `cb_oru_subsidy` when `C=0` |

#### Expected Acceptance Result

**ACCEPT** — valid genesis block; subsidy equals `R(0)`.

---

### GV-ORU-0002 — Early Block Subsidy & ORU Convergence Burn

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-ORU-0002` |
| **Category** | GV-002 |
| **Status** | **DRAFT** |
| **Description** | Height-1 block: full early subsidy with non-zero Converge destruction |
| **Spec refs** | SPEC-002 §4.1, §4.3; SPEC-005 CI-020, CI-021; `MIN_CONVERGENCE_AMOUNT` |

#### Input

| Field | Value |
|-------|-------|
| `height` | `1` |
| `parent_tip` | Genesis at height `0` with `S_ORU(0) = 1000 ORU` |
| `converge_burn_sum_C(h)` | `100 ORU` (= `10_000_000_000` atoms) — sum of valid Converge destructions in block |
| `valid_block` | Yes — PoW/header/tx rules satisfied; converge txs valid |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `0` |
| `S_ORU(0)` | `100_000_000_000` atoms (`1000 ORU`) |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `1` |
| `S_ORU(1)` | `190_000_000_000` atoms (`1900 ORU`) |
| `S_RES(1)` | unchanged from parent (no RES mint in this vector) |

#### Expected ORU Subsidy & Burn Deltas

| Field | Atoms | ORU |
|-------|-------|-----|
| `R(1)` | `100_000_000_000` | `1000` |
| `C(1)` | `10_000_000_000` | `100` |
| `Δ_ORU` | `+90_000_000_000` | `+900` |

Recurrence check: `1000 + 1000 − 100 = 1900 ORU`.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **ORU-L1** | `oru_supply_cumulative ≥ 0` | **PASS** |
| **ORU-L2** | `oru_converge_burn_block ≥ 0` | **PASS** |
| **ORU-L3** | `S_ORU(h) = S_ORU(h−1) + R(h) − C(h)` | **PASS** |
| **ORU-L4** | `oru_subsidy_block = GetBlockSubsidy(1)` | **PASS** (`1000 ORU`) |
| **CI-020** | `S_ORU(h) ≥ 0` | **PASS** |
| **CI-021** | ORU recurrence preservation | **PASS** |

#### Expected Runtime Witness Values (FVR-B / FVR-C)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `1` |
| `oru_supply_parent` | `rs_oru_supply_parent` | `100000000000` |
| `oru_supply_cumulative` | `rs_oru_supply` | `190000000000` |
| `oru_subsidy_block` | `cb_oru_subsidy` | `100000000000` |
| `oru_converge_burn_block` | `cb_oru_burn_delta` | `10000000000` |
| `res_mint_block` | `cb_res_mint_delta` | `0` |
| `res_mint_count_block` | `res_mint_count_M_h` | `0` |

FVR-E: `passed = true`, `violations = []`.

#### Expected Coq Values

| Field | Expected | Scaffold note |
|-------|----------|---------------|
| `rs_oru_supply_parent` | `100000000000` | — |
| `rs_oru_supply` (authoritative target) | `190000000000` | Full RFC-002 recurrence |
| `cb_oru_subsidy` | `100000000000` | — |
| `cb_oru_burn_delta` | `10000000000` | Runtime hook export |
| `cb_oru_mint_delta` (scaffold) | `100000000000` | **Subsidy-only** — scaffold yields `200000000000` |

**Known Coq gap (documented, not a vector defect):** `ConnectBlockMint.v` defines
`rs_oru_supply' = rs_oru_supply + cb_oru_mint_delta` with `cb_oru_mint_delta =
cb_oru_subsidy` (no burn). Vector expected `rs_oru_supply` follows SPEC-002;
Coq scaffold refinement is a Phase G proof obligation.

#### Expected Acceptance Result

**ACCEPT** — valid block; subsidy `R(1)`; `C(1)` correctly sums converge destruction.

---

### GV-ORU-0003 — Post-Halving Block Subsidy & ORU Convergence Burn

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-ORU-0003` |
| **Category** | GV-002 |
| **Status** | **DRAFT** |
| **Description** | Height at subsidy floor (`125 ORU`) after three halving shifts, with converge burn |
| **Spec refs** | SPEC-002 §4.1, §4.4; SPEC-005 CI-020, CI-021; `ORU_MIN_BLOCK_SUBSIDY` |

#### Input

| Field | Value |
|-------|-------|
| `height` | `630_000` |
| `parent_tip` | Height `629_999` (declared parent state below) |
| `converge_burn_sum_C(h)` | `25 ORU` (= `2_500_000_000` atoms) |
| `valid_block` | Yes |

**Halving justification:** `⌊630_000 / 210_000⌋ = 3`; capped at `ORU_SUBSIDY_MAX_SHIFTS`;
`1000 >> 3 = 125 ORU`.

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `629_999` |
| `S_ORU(629_999)` | `500_000_000_000_000` atoms (`5_000_000 ORU`) — declared parent fixture |

> Parent cumulative is a **vector fixture** (self-contained recurrence input). It
> is not derived from full-chain replay in this task.

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `630_000` |
| `S_ORU(630_000)` | `500_010_000_000_000` atoms (`5_000_100 ORU`) |

#### Expected ORU Subsidy & Burn Deltas

| Field | Atoms | ORU |
|-------|-------|-----|
| `R(630_000)` | `12_500_000_000` | `125` |
| `C(630_000)` | `2_500_000_000` | `25` |
| `Δ_ORU` | `+10_000_000_000` | `+100` |

Recurrence check: `5_000_000 + 125 − 25 = 5_000_100 ORU`.

#### Expected Invariants

| Id | Statement | Expected |
|----|-----------|----------|
| **ORU-L1** | `oru_supply_cumulative ≥ 0` | **PASS** |
| **ORU-L2** | `oru_converge_burn_block ≥ 0` | **PASS** |
| **ORU-L3** | `S_ORU(h) = S_ORU(h−1) + R(h) − C(h)` | **PASS** |
| **ORU-L4** | `oru_subsidy_block = GetBlockSubsidy(630000)` | **PASS** (`125 ORU`) |
| **CI-020** | `S_ORU(h) ≥ 0` | **PASS** |
| **CI-021** | ORU recurrence preservation | **PASS** |

#### Expected Runtime Witness Values (FVR-B / FVR-C)

| Mirror field | Coq hook | Expected value |
|--------------|----------|----------------|
| `height` | `height` | `630000` |
| `oru_supply_parent` | `rs_oru_supply_parent` | `500000000000000` |
| `oru_supply_cumulative` | `rs_oru_supply` | `500010000000000` |
| `oru_subsidy_block` | `cb_oru_subsidy` | `12500000000` |
| `oru_converge_burn_block` | `cb_oru_burn_delta` | `2500000000` |
| `res_mint_block` | `cb_res_mint_delta` | `0` |
| `res_mint_count_block` | `res_mint_count_M_h` | `0` |

FVR-E: `passed = true`, `violations = []`.

#### Expected Coq Values

| Field | Expected | Scaffold note |
|-------|----------|---------------|
| `rs_oru_supply_parent` | `500000000000000` | — |
| `rs_oru_supply` (authoritative target) | `500010000000000` | Full RFC-002 recurrence |
| `cb_oru_subsidy` | `12500000000` | — |
| `cb_oru_burn_delta` | `2500000000` | Runtime hook export |
| `cb_oru_mint_delta` (scaffold) | `12500000000` | **Subsidy-only** — scaffold yields `500012500000000` |

Same Coq burn gap as GV-ORU-0002; documented for Phase G refinement.

#### Expected Acceptance Result

**ACCEPT** — valid block at subsidy floor; `R(630_000) = 125 ORU`; converge burn applied.

---

## 4. Vector Authority

Per [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) §4:

| Rule | Application |
|------|-------------|
| **Normative** | When promoted to `ESTABLISHED`, all layers MUST match declared outputs on declared inputs. |
| **Runtime** | `GetBlockSubsidy`, `m_chain_oru_total`, and FVR mirrors MUST conform. Witnesses are evidence only. |
| **Coq** | Proofs MUST target vector `rs_oru_supply` (full recurrence); scaffold gap is explicit debt, not vector amendment. |
| **No back-flow** | Runtime observations MUST NOT modify vector expected values. |
| **Status** | Entries are **DRAFT** until independently confirmed (review + harness pass) → **ESTABLISHED**. |

---

## 5. Status Lifecycle

| Vector | PROOF-000 (prior) | This task | Next gate |
|--------|-------------------|-----------|-----------|
| GV-ORU-0001 | RESERVED | **DRAFT** | Phase F runtime harness + review → ESTABLISHED |
| GV-ORU-0002 | RESERVED | **DRAFT** | Phase F + Coq burn refinement (Phase G) |
| GV-ORU-0003 | RESERVED | **DRAFT** | Phase F + Coq burn refinement (Phase G) |

**Promotion criteria (DRAFT → ESTABLISHED):**

1. Declared values trace to SPEC-002 recurrence and frozen subsidy constants.
2. Runtime mirror capture on conforming fixture blocks matches all witness fields.
3. FVR-E reports `passed = true` on captured mirrors.
4. Coq subsidy fields match; burn gap acknowledged in theorem statement.
5. No specification amendment required.

---

## 6. Relationships

### SPEC-002 — ORU recurrence

All three vectors instantiate:

\[
S_{\text{ORU}}(h+1) = S_{\text{ORU}}(h) + R(h+1) - C(h+1)
\]

with `R(h)` from the frozen subsidy schedule and `C(h)` from valid Converge
destruction sums.

### SPEC-005 — Economic invariants

| Invariant | Vectors |
|-----------|---------|
| **CI-020** (non-negativity) | All three — strictly positive `S_ORU` after step |
| **CI-021** (recurrence) | All three — primary check |

CI-022..CI-024 (RES) are **N/A** for these ORU-only vectors (`M(h) = 0`).

### SPEC-006 — Layer mapping

| Vector field | Coq family | Runtime (FVR) |
|--------------|------------|---------------|
| `S_ORU` | `rs_oru_supply` | `oru_supply_cumulative` |
| `R(h)` | `cb_oru_subsidy` | `oru_subsidy_block` |
| `C(h)` | `cb_oru_burn_delta` (hook) | `oru_converge_burn_block` |
| Recurrence | `oru_supply_equation_theorems` | ORU-L3 |

### FVR-A / FVR-B — Runtime mirror & invariants

| Layer | Role |
|-------|------|
| **FVR-A** | Pre-export sanity on mirror |
| **FVR-B** | Post-connect `RuntimeSupplyState` capture |
| **FVR-C** | Coq field hook projection (`rs_oru_supply`, `cb_oru_subsidy`, `cb_oru_burn_delta`) |
| **FVR-E** | ORU-L1..L4 active verification |

Witness exports MUST match expected witness tables when run on conforming inputs.
Witness pass does **not** establish equivalence (`equivalence_claimed = false`).

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
| ORU Golden Vector Framework | **ESTABLISHED** |
| Vector Entries GV-ORU | **DRAFT** |
| Proof Infrastructure | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-002` — GV-ORU-0001..0003 populated (DRAFT); frozen subsidy schedule documented |
