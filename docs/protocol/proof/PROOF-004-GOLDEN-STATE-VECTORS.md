# PROOF-004: Golden State Consensus Vectors

**Task:** `TASK-PROOF-005`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (framework); vector entries **DRAFT**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md), [PROOF-001](./PROOF-001-GOLDEN-ORU-VECTORS.md), [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md), [PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md), SPEC-001, SPEC-002  
**Category:** GV-006 (State Transition), GV-008 (Reorg Determinism)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document populates the initial **State Golden Consensus Vectors**
`GV-STATE-0001` … `GV-STATE-0003` with canonical per-block state evolution:
`State(h) → State(h+1)` including chain tip, height, ORU/RES supplies, and UTXO
deltas.

```
Specification (SPEC-001 State(h), SPEC-002 supply laws)
        ↓
Formal Model (transition function, UTXO spend/create)
        ↓
Coq Proof Model (ReplayState / ReplaySnapshot)
        ↓
Bitcoin-v25 Runtime (ConnectBlock / chain caches / FVR mirrors)
        ↓
Runtime Witnesses (FVR-A..N — evidence only)
```

**Honesty boundaries:**

| Claim | Status |
|-------|--------|
| Global Coq ≡ Runtime | **NOT ESTABLISHED** |
| Draw equivalence / closure | **PROHIBITED** (FVR-M) |
| `state_hash` as normative truth | **NO** — observational projection only |
| **Equivalence Claimed** | **false** |

This task is **proof infrastructure only**. No consensus, validation, state_hash,
FVR, or monetary-policy changes.

---

## 1. Purpose

Establish normative **full-state** vector mappings across verification layers.

| Layer | State obligation |
|-------|------------------|
| **Specification** | `S(h)` tuple ([SPEC-001](../../specification/SPEC-001-STATE-MODEL.md) §3); supply recurrences ([SPEC-002](../../specification/SPEC-002-ECONOMIC-MODEL.md)) |
| **Formal model** | Deterministic `Transition(S, B)`; UTXO spend/create; reorg rollback ([SPEC-005](../../specification/SPEC-005-CONSENSUS-INVARIANTS.md) CI-012..CI-014) |
| **Coq** | `ReplayState` (`rs_utxo`, `rs_chain`, `rs_oru_supply`, `rs_res_supply`, `rs_history`) |
| **Runtime** | `CBlockIndex` caches, `CCoinsViewCache`, FVR-B mirrors |
| **Witnesses** | FVR-A..N exports — corroboration only |

**Composition:** GV-STATE vectors **integrate** supply and draw fixtures from
[PROOF-001](./PROOF-001-GOLDEN-ORU-VECTORS.md), [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md), and
[PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md) into end-to-end state projections.

**UTXO notation:** Outpoints written `(txid[n], index)`; values in base atoms.
Script/asset classification is derived from `scriptPubKey` (no parallel asset field).

---

## 2. Invariant Applicability Catalog

Vectors reference the authoritative CI catalog (SPEC-005). Not every CI applies to
every fixture; per-vector matrices use **PASS**, **N/A**, or **PRIMARY** (main
check for that vector).

| Range | IDs | Domain |
|-------|-----|--------|
| Consensus | CI-001..CI-003 | Chain, fork choice, validation determinism |
| State | CI-010..CI-014 | UTXO, transition, replay, reorg |
| Economic | CI-020..CI-024 | ORU/RES supply |
| Draw | CI-030..CI-033 | Convergence draw (classification only; no equivalence) |
| Freeze | CI-040..CI-042 | Immutable parameters |

**Runtime labels:** ORU-L1..L4, RES-L1..L7 (RES-L7 = cap headroom per [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md)).

---

## 3. Vector Registry Entries

### GV-STATE-0001 — Genesis Block State (Initial UTXO, ORU, RES)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-STATE-0001` |
| **Category** | GV-006 |
| **Status** | **DRAFT** |
| **Description** | Chain origin: genesis `State(0)` with initial coinbase UTXO and subsidy-only ORU |
| **Composes** | [GV-ORU-0001](./PROOF-001-GOLDEN-ORU-VECTORS.md) |
| **Spec refs** | SPEC-001 §3; CI-001, CI-012, CI-020, CI-022 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `0` |
| `block_role` | Genesis |
| `block_header` | `prev_hash = 0³²`, `height = 0`, valid PoW/nBits/time per chainparams |
| `transactions` | `[coinbase₀]` — single coinbase crediting `R(0)` |
| `parent_tip` | `null` |

**Fixture txid (genesis coinbase):** `GENESIS_CB` = `11…11` (32 bytes; placeholder id)

#### Expected State Before — `S(pre)` (chain origin)

| Field | Value |
|-------|-------|
| `ChainTip` | — (undefined) |
| `BlockHeight` | — |
| `S_ORU` | `0` |
| `S_RES` | `0` |
| `UTXO` | `∅` (empty) |

#### Expected State After — `S(0)`

| Field | Value |
|-------|-------|
| `ChainTip` | `block_hash(GENESIS)` = `GENESIS_TIP` (fixture `22…22`) |
| `BlockHeight` | `0` |
| `S_ORU` | `100_000_000_000` atoms (`1000 ORU`) |
| `S_RES` | `0` |
| `UTXO` | `{(GENESIS_CB, 0) ↦ 100_000_000_000 ORU atoms, P2WPKH}` |
| `UTXO_count` | `1` |

#### Supply & Mint Deltas

| Field | Atoms |
|-------|-------|
| `R(0)` | `100_000_000_000` |
| `C(0)` | `0` |
| `res_mint_block` | `0` |
| `res_mint_count_block` | `0` |
| `Δ_ORU` | `+100_000_000_000` |
| `Δ_RES` | `0` |

#### Invariant Matrix (selected)

| Id | Result |
|----|--------|
| CI-001 | **PASS** — genesis root |
| CI-010..CI-011 | **N/A** — no spends |
| CI-012 | **PRIMARY** — unique `S(0)` |
| CI-013 | **N/A** — single block |
| CI-020, CI-021 | **PASS** |
| CI-022 | **PASS** — `S_RES(0)=0` |
| CI-030..CI-033 | **N/A** — no draw |
| CI-040..CI-042 | **PASS** — frozen params |
| ORU-L1..L4 | **PASS** |
| RES-L1..L7 | **PASS** (trivial zero RES) |

#### Runtime Witness Fields (FVR-A..N)

| Layer | Role | Expected |
|-------|------|----------|
| **FVR-A** | Pre-export checks | pass |
| **FVR-B** | Supply mirror | `oru_supply_cumulative=100000000000`, `res_mint_cumulative=0` |
| **FVR-C** | Coq hook | `rs_oru_supply=100000000000`, `cb_oru_subsidy=100000000000` |
| **FVR-E** | Invariants | `passed=true`, 10 checked |
| **FVR-F** | Witness chain | genesis witness at h=0 |
| **FVR-G..N** | Alignment catalogs | evidence only; `equivalence_claimed=false` |

#### Coq Hook Values (`ReplayState`)

| Field | Expected |
|-------|----------|
| `rs_chain.cm_height` | `0` |
| `rs_chain.cm_tip_hash` | `hash32_zero` (placeholder genesis) |
| `rs_oru_supply` | `100000000000` |
| `rs_res_supply` | `0` |
| `rs_utxo` | one entry: genesis coinbase |
| `rs_history` | `[]` |

#### Expected Acceptance Result

**ACCEPT** — valid genesis block.

---

### GV-STATE-0002 — Normal Transaction Block (Multi-UTXO, ORU Burn, RES Mint)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-STATE-0002` |
| **Category** | GV-006 |
| **Status** | **DRAFT** |
| **Description** | Height-10 block: coinbase + converge burn + draw-authorized RES mint |
| **Composes** | [GV-ORU-0002](./PROOF-001-GOLDEN-ORU-VECTORS.md) (burn), [GV-RES-0001](./PROOF-002-GOLDEN-RES-VECTORS.md), [GV-DRAW-0001](./PROOF-003-GOLDEN-DRAW-VECTORS.md) (draw at h=10) |
| **Spec refs** | CI-010..CI-012, CI-020..CI-024, CI-030..CI-033 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `10` |
| `block_header` | `prev_hash = TIP(9)`, `height = 10`, consecutive chain |
| `transactions` | `[coinbase₁₀, converge_burn_tx, res_mint_tx]` |
| `draw_won` | `true` |
| `converge_burn_C(10)` | `50 ORU` = `5_000_000_000` atoms |
| `valid_block` | Yes |

**Fixture ids:**

| Tx | Role |
|----|------|
| `CB_10` | Coinbase subsidy `R(10)=1000 ORU` |
| `CV_10` | Converge: spends `(FUND_A, 0)`, destroys `50 ORU` |
| `RM_10` | RES mint: creates `(RM_10, 0)` with `10_000` RES atoms |

#### Expected State Before — `S(9)`

| Field | Value |
|-------|-------|
| `ChainTip` | `TIP(9)` = `33…33` |
| `BlockHeight` | `9` |
| `S_ORU` | `910_000_000_000` atoms (`9100 ORU`) — fixture parent |
| `S_RES` | `0` |
| `UTXO` (minimal) | `{(GENESIS_CB,0) ↦ 500B ORU}, {(FUND_A,0) ↦ 100B ORU}, {(FUND_B,0) ↦ 310B ORU}` |
| `UTXO_count` | `3` |

#### Expected State After — `S(10)`

| Field | Value |
|-------|-------|
| `ChainTip` | `TIP(10)` = `44…44` |
| `BlockHeight` | `10` |
| `S_ORU` | `1_005_000_000_000` atoms (`10050 ORU`) |
| `S_RES` | `10_000` atoms (`100.00 RES`) |
| `UTXO` (delta summary) | Spend `(FUND_A,0)`; add coinbase outputs + `(RM_10,0)` RES |
| `UTXO_count` | `4` (fixture: −1 spent, +2 created net) |

**Recurrence:** `9100 + 1000 − 50 = 10050 ORU`; `0 + 10000 × 1 = 10000 RES`.

#### Supply & Mint Deltas

| Field | Atoms |
|-------|-------|
| `R(10)` | `100_000_000_000` |
| `C(10)` | `5_000_000_000` |
| `res_mint_block` | `10_000` |
| `res_mint_count_block` | `1` |
| `Δ_ORU` | `+95_000_000_000` |
| `Δ_RES` | `+10_000` |

#### Invariant Matrix (selected)

| Id | Result |
|----|--------|
| CI-001 | **PASS** — `prev_hash` links `TIP(9)` |
| CI-010 | **PRIMARY** — `(FUND_A,0)` existed before spend |
| CI-011 | **PASS** — no duplicate spends |
| CI-012 | **PRIMARY** — unique `S(10)` |
| CI-020..CI-021 | **PASS** |
| CI-022..CI-024 | **PASS** |
| CI-030..CI-031 | **PASS** — draw fixture (no equivalence claim) |
| CI-032 | **PASS** — frozen `T` at ConnectBlock |
| CI-033 | **PRIMARY** — mint follows valid draw |
| CI-040..CI-042 | **PASS** |
| ORU-L1..L4 | **PASS** |
| RES-L1..L7 | **PASS** |

#### Runtime Witness Fields (FVR-A..N)

| Field / Layer | Expected |
|---------------|----------|
| `height` | `10` |
| `oru_supply_parent` / `rs_oru_supply_parent` | `910000000000` |
| `oru_supply_cumulative` / `rs_oru_supply` | `1005000000000` |
| `oru_converge_burn_block` / `cb_oru_burn_delta` | `5000000000` |
| `oru_subsidy_block` / `cb_oru_subsidy` | `100000000000` |
| `res_mint_cumulative` / `rs_res_supply` | `10000` |
| `res_mint_block` / `cb_res_mint_delta` | `10000` |
| `res_mint_count_block` / `res_mint_count_M_h` | `1` |
| **FVR-F** | witness child h=10 → parent h=9 linkage |
| **FVR-M** | draw CES blocked; supply scalars comparable |

FVR-E: `passed=true`, `violations=[]`.

#### Coq Hook Values

| Field | Expected |
|-------|----------|
| `rs_chain.cm_height` | `10` |
| `rs_oru_supply` | `1005000000000` |
| `rs_res_supply` | `10000` |
| `cb_oru_subsidy` | `100000000000` |
| `cb_oru_burn_delta` | `5000000000` (hook; scaffold gap per PROOF-001) |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |
| `cm_draw_won` | `true` |
| `rs_utxo` | abstract list matching spend/create summary |

#### Expected Acceptance Result

**ACCEPT** — normal multi-tx block with ORU burn and single RES mint.

---

### GV-STATE-0003 — Shallow Reorg (Supply Preservation & Branch Determinism)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-STATE-0003` |
| **Category** | GV-006, GV-008 |
| **Status** | **DRAFT** |
| **Description** | Two height-6 tips from common ancestor h=4; reorg to higher-work branch B |
| **Spec refs** | CI-002, CI-014; CI-012, CI-020..CI-024 |

#### Input

| Field | Value |
|-------|-------|
| `common_ancestor_height` | `4` |
| `stale_tip` | Branch **A** at height `6` (lower work) |
| `winning_tip` | Branch **B** at height `6` (greater chainwork) |
| `reorg_depth` | `2` (disconnect A blocks 5–6, connect B blocks 5'–6') |
| `valid_reorg` | Yes |

**Branch summary:**

| Branch | Block 5 | Block 6 | `S_ORU(6)` | `S_RES(6)` | `M(6)` |
|--------|---------|---------|------------|------------|--------|
| **A** (stale) | subsidy only | subsidy + RES mint | `5_850_000_000_000` | `10_000` | `1` on blk6 |
| **B** (winning) | subsidy + `C=25 ORU` | subsidy only | `5_900_000_000_000` | `0` | `0` |

#### Expected State Before Reorg — Active Tip **A** at `S(6)`

| Field | Value |
|-------|-------|
| `ChainTip` | `TIP_A(6)` = `55…55` |
| `BlockHeight` | `6` |
| `S_ORU` | `5_850_000_000_000` atoms |
| `S_RES` | `10_000` atoms |
| `UTXO` | includes `(RM_6A, 0)` RES from branch A mint |
| `rs_history` depth | `≥ 2` snapshots for rollback |

#### Expected State After Reorg — Active Tip **B** at `S(6)`

| Field | Value |
|-------|-------|
| `ChainTip` | `TIP_B(6)` = `66…66` |
| `BlockHeight` | `6` |
| `S_ORU` | `5_900_000_000_000` atoms |
| `S_RES` | `0` atoms |
| `UTXO` | **no** `(RM_6A, 0)`; reflects branch B spend/create only |
| `UTXO_count` | fixture `3` |

**CI-014 check:** Post-reorg `S(6)` equals sequential replay of branch B from `S(4)`.

#### Supply Deltas (Winning Branch B Only)

| Step | `Δ_ORU` | `Δ_RES` | `M` |
|------|---------|---------|-----|
| Block 5' | `+1000 ORU − 25 ORU` | `0` | `0` |
| Block 6' | `+1000 ORU` | `0` | `0` |
| **Net B → h=6** | vs `S(4)=4_000_000_000_000`: `+1_900_000_000_000` | `0` | — |

Parent `S(4)` fixture: `S_ORU = 4_000_000_000_000`, `S_RES = 0`.

#### Invariant Matrix (selected)

| Id | Result |
|----|--------|
| CI-001 | **PASS** — both branches extend `S(4)` |
| CI-002 | **PRIMARY** — unique active tip after reorg |
| CI-012 | **PASS** — deterministic per branch |
| CI-013 | **PASS** — replay B yields post-reorg state |
| CI-014 | **PRIMARY** — reorg ≡ branch replay |
| CI-020..CI-024 | **PASS** on winning branch |
| CI-030..CI-033 | **N/A** on B (no mint); A mint rolled back |
| CI-040..CI-042 | **PASS** |
| ORU-L1..L4 | **PASS** on winning tip mirror |
| RES-L1..L7 | **PASS** — `S_RES=0` after rollback of A mint |

#### Runtime Witness Fields (FVR-A..N)

| Layer | Expected |
|-------|----------|
| **FVR-B** | Mirror at `TIP_B(6)`: `rs_oru_supply=5900000000000`, `rs_res_supply=0` |
| **FVR-F** | Witness chain rewound: h=6 witness matches B branch only |
| **FVR-K..L** | Global/closure catalogs: draw blocked; supply partial alignment only |
| **FVR-N** | Phase-5 summary: `equivalence_claimed=false` |

Stale branch A mirrors are **invalidated** after reorg (non-authoritative).

#### Coq Hook Values

| Field | Expected |
|-------|----------|
| `rs_oru_supply` | `5900000000000` |
| `rs_res_supply` | `0` |
| `rs_chain.cm_height` | `6` |
| `rs_history` | snapshots restored via rollback to `S(4)` then B replay |
| `cb_res_mint_delta` | `0` at final tip |

Coq `replay_reorg` / `rollback` lemmas are **targets** (PD-4C family), not closure.

#### Expected Acceptance Result

**ACCEPT** (reorg operation) — valid reorg to strictly greater chainwork tip B.

**REJECT** if attempted to retain branch A as active after B supersedes it.

---

## 4. Vector Authority

Per [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) §4:

| Rule | Application |
|------|-------------|
| **Hierarchy** | Specification > Vectors > Implementation > Witnesses |
| **Normative** | `ESTABLISHED` vectors fix `State(h)` projections on declared inputs |
| **UTXO** | Fixture outpoints are normative for vector comparison; full script proofs are out of scope |
| **state_hash** | May be computed for audit; **must not** override vector authority |
| **Draw** | CI-030..CI-033 cited for classification; FVR-M prohibits draw equivalence |
| **Global equivalence** | **NOT CLAIMED** |

---

## 5. Status Lifecycle

| Vector | PROOF-000 (prior) | This task | Next gate |
|--------|-------------------|-----------|-----------|
| GV-STATE-0001 | RESERVED | **DRAFT** | Phase F genesis fixture harness |
| GV-STATE-0002 | RESERVED | **DRAFT** | Phase F multi-tx + FVR-F chain |
| GV-STATE-0003 | RESERVED | **DRAFT** | Reorg functional replay cross-check |

**Promotion (DRAFT → ESTABLISHED):**

1. `State(h)` projections trace to SPEC-001 §3 and SPEC-002 recurrences.
2. Supply fields match composed ORU/RES/DRAW vectors.
3. FVR-B/F/E witnesses match on ACCEPT paths.
4. Reorg vector satisfies CI-014 on winning branch replay.
5. `equivalence_claimed` remains `false`.

---

## 6. Relationships

### SPEC-001 / SPEC-002

| Concept | Vectors |
|---------|---------|
| `State(h)` tuple | All three |
| ORU recurrence | GV-STATE-0001, GV-STATE-0002, GV-STATE-0003 (branch B) |
| RES recurrence | GV-STATE-0002 (mint); GV-STATE-0003 (rollback removes mint) |
| UTXO map | All three |

### FVR-A..N (observational map)

| Layer | State vector role |
|-------|-------------------|
| **FVR-A** | Pre-export sanity |
| **FVR-B** | Post-connect `RuntimeSupplyState` |
| **FVR-C** | Coq field hooks |
| **FVR-D** | Diagnostic export |
| **FVR-E** | ORU-L / RES-L checks |
| **FVR-F** | Witness chain per height |
| **FVR-G..J** | Coq catalog / candidate equivalence |
| **FVR-K..L** | Global candidate / closure gate |
| **FVR-M** | Draw impossibility (unchanged) |
| **FVR-N** | Phase-5 summary snapshot |

### CI-001..CI-042

Full catalog in §2. **PRIMARY** checks per vector:

| Vector | Primary CIs |
|--------|-------------|
| GV-STATE-0001 | CI-012, CI-020, CI-022 |
| GV-STATE-0002 | CI-010..CI-012, CI-020..CI-024, CI-033 |
| GV-STATE-0003 | CI-002, CI-014, CI-012 |

Draw CIs (CI-030..CI-033) apply where draw/mint occurs; equivalence remains prohibited.

### Cross-vector composition

```text
GV-STATE-0001  ⊂  genesis slice
GV-STATE-0002  ⊃  GV-ORU-0002 burn + GV-RES-0001 mint + GV-DRAW-0001 (h=10)
GV-STATE-0003  ⊃  GV-008 reorg determinism (shallow, depth 2)
```

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
| FVR-M draw impossibility | **Preserved** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## Task Output Status

| Field | Status |
|-------|--------|
| State Golden Vector Framework | **ESTABLISHED** |
| Vector Entries GV-STATE | **DRAFT** |
| Proof Infrastructure | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-005` — GV-STATE-0001..0003 populated (DRAFT); composes ORU/RES/DRAW vectors |
