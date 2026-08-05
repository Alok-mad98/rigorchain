# PROOF-003: Golden Draw Consensus Vectors

**Task:** `TASK-PROOF-004`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (framework); vector entries **DRAFT**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md), [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md), SPEC-001, SPEC-002, SPEC-005  
**Categories:** GV-004 (Draw Eligibility), GV-005 (Draw Winner Selection)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document populates the initial **Draw Golden Consensus Vectors**
`GV-DRAW-0001` … `GV-DRAW-0003` with canonical expected values for convergence-draw
events, winner assignment, and RES mint interaction.

```
Specification (SPEC-003 draw rules, SPEC-002 RES coupling)
        ↓
Formal Model (RFC-000 σ, C, inequality, winner)
        ↓
Coq Proof Model (Kernel / Execute / DrawTrace)
        ↓
Bitcoin-v25 Runtime (RunSelection / ConnectBlock draw path)
        ↓
Runtime Witnesses (FVR — evidence only; draw CES blocked per FVR-M)
```

**Honesty boundary (FVR-M):**

| Claim | Status |
|-------|--------|
| Draw equivalence (Coq ≡ Runtime draw) | **PROHIBITED** — permanently non-closable |
| Draw closure under Phase-5 predicate | **IMPOSSIBLE** (proven) |
| **Equivalence Claimed** | **false** |

Vectors are **comparison targets** and **classification fixtures**. They do
not establish draw equivalence or global Coq ≡ Runtime.

This task is **proof infrastructure only**. No consensus, validation, state_hash,
FVR, or monetary-policy changes.

---

## 1. Purpose

Establish normative draw vector mappings across all verification layers for:

- Candidate set `F` construction and convergence root `C`
- Per-txid σ digests and eligibility (GV-004)
- Deterministic single-winner selection (GV-005)
- RES mint coupling when draw authorizes mint (`M(h)`, CI-033)

| Layer | Draw obligation |
|-------|----------------|
| **Specification** | CR-030..CR-045; CI-030..CI-033 ([SPEC-003](../../specification/SPEC-003-CONSENSUS-RULES.md), [SPEC-005](../../specification/SPEC-005-CONSENSUS-INVARIANTS.md)) |
| **Formal model** | `C = ConvergeRoot(F)`; `σ_i = Hash256(txid_i ‖ C ‖ bind_commit ‖ LE32(h))`; `H_i < T·f_i`; argmin winner |
| **Coq** | `KernelSelection`, `KernelEligibility`, `Execute`, `DrawTrace.convergence_root_bytes` |
| **Runtime** | `RunSelection`, `SelectConvergenceDrawWinnerFromCandidates` |
| **Witnesses** | FVR exports observe supply/draw-adjacent fields; **not authoritative** |

**Kernel fixture anchor:** Draw outputs in GV-DRAW-0001/0002 align with
`formal_verification/phase3_golden_vectors.json` rows **WIN-002** and **WIN-004**
(PHASE-3 cross-implementation MATCH). Production ConnectBlock binds
`T = RES_PROBABILITY_TARGET` (CI-032); kernel fixtures declare `T_draw` in vector
input for byte-exact regression.

---

## 2. Frozen Draw Parameters

| Symbol | Value | Role |
|--------|-------|------|
| `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` | `1` | Minimum `|F|` for draw participation |
| `CONVERGENCE_DRAW_BIND_DEPTH` | `10` (`k`) | Bind ancestor depth for `bind_commit` |
| `RES_PROBABILITY_TARGET` | `00010000…00` (2^240) | **Production** inequality RHS (CI-032) |
| `RES_REWARD_AMOUNT` | `10_000` atoms | Mint when draw authorizes RES |
| `RES_MAX_SUPPLY` | `2_100_000_000` atoms | Cap (CI-023) |

**Convergence root boundary (RFC-015):** `|F| < 2` ⇒ `C = 32` zero bytes; `|F| ≥ 2`
⇒ `C = Hash256(concat(sort(txid)))`.

**Single-winner rule (RFC-000 §9):** Argmin σ cross-product among eligible; txid
lex tie-break on equality.

---

## 3. Vector Registry Entries

### GV-DRAW-0001 — First Valid Draw (Minimal Eligible `F`)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-DRAW-0001` |
| **Categories** | GV-004, GV-005 |
| **Status** | **DRAFT** |
| **Description** | Minimal `|F| = 1`: single eligible candidate wins; RES mint authorized |
| **Phase-3 anchor** | `WIN-002` (`RunSelection` / `single`) |
| **Spec refs** | CI-030, CI-031, CI-033; CR-041, CR-042, CR-043 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `1` |
| `bind_block_hash` | `bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb` |
| `target_prev` | `0` (fixture; bound into σ via `bind_commit`) |
| `T_draw` (kernel fixture RHS) | `ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` |
| `F` (txids, fees/weights `f`) | `[{ txid: aaaa…aa, f: 1 }]` |
| `|F|` | `1` |
| `draw_won` | `true` |
| `eligible_tx` | `aaaa…aa` (index `0`) |
| `valid_block` | Yes — draw authorizes single RES mint |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `0` (parent) |
| `S_RES(0)` | `0` atoms |
| `S_ORU(0)` | per parent chain (ORU orthogonal to this vector) |

#### Expected Draw Outputs (Kernel)

| Field | Value |
|-------|-------|
| `C` | `0000000000000000000000000000000000000000000000000000000000000000` |
| `σ[0]` | `bddf6120502a4dc1fb3cc6fadd0c21d0df9612f46d2d879a16251b86821ddb23` |
| `eligible[0]` | `true` |
| `winner_index` | `0` |
| `winner_txid` | `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` |

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `1` |
| `S_RES(1)` | `10_000` atoms (`100.00 RES`) |
| `M(1)` | `1` |

#### Expected RES Mint Delta

| Field | Atoms |
|-------|-------|
| `res_mint_block` | `10_000` |
| `res_mint_count_block` | `1` |
| `Δ_RES` | `+10_000` |

#### Expected Invariants

| Id | Expected |
|----|----------|
| **CI-030** (draw determinism) | **PASS** — fixed kernel outputs |
| **CI-031** (single winner) | **PASS** — one winner index |
| **CI-032** (frozen `T`) | **PASS** at ConnectBlock (`RES_PROBABILITY_TARGET`); kernel uses `T_draw` fixture |
| **CI-033** (mint requires valid draw) | **PASS** — mint follows authorized winner |
| **CI-022** | **PASS** — `0 + 10_000 × 1` |
| **CI-024** | **PASS** — `M(1) = 1` |
| **RES-L1..L6** | **PASS** (see [PROOF-002](./PROOF-002-GOLDEN-RES-VECTORS.md)) |
| **RES-L7** (cap headroom) | **PASS** |

#### Expected Runtime Witness Values (FVR-B / FVR-C / FVR-F)

| Field | Expected |
|-------|----------|
| `height` | `1` |
| `res_mint_parent` / `rs_res_supply_parent` | `0` |
| `res_mint_cumulative` / `rs_res_supply` | `10000` |
| `res_mint_block` / `cb_res_mint_delta` | `10000` |
| `res_mint_count_block` / `res_mint_count_M_h` | `1` |

FVR-F: witness chain child at h=1 links to parent `rs_res_supply = 0`.

**FVR-M classification:** Draw component remains `CES_FundamentallyBlocked` for
equivalence closure; supply scalars may be `CES_Comparable` independently.

#### Expected Coq Hook Values

| Field | Expected |
|-------|----------|
| `convergence_root_bytes` | 32 zero bytes |
| `cm_draw_won` | `true` |
| `rs_res_supply` | `10000` |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |
| `Execute` / kernel winner | `Some` winner matching `aaaa…aa` |

Coq kernel refinement is a **target** (PD-12/PD-18 family), not a closure claim.

#### Expected Acceptance Result

**ACCEPT** — valid block; draw selects single winner; RES mint at `RES_REWARD_AMOUNT`.

---

### GV-DRAW-0002 — Multi-tx Draw with Single Winner

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-DRAW-0002` |
| **Categories** | GV-004, GV-005 |
| **Status** | **DRAFT** |
| **Description** | Two candidates, both eligible; higher weight (`f`) wins |
| **Phase-3 anchor** | `WIN-004` (`RunSelection` / `wgt`) |
| **Spec refs** | CI-030, CI-031; CR-041, CR-042 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `7` |
| `bind_block_hash` | `dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd` |
| `target_prev` | `0` (fixture) |
| `T_draw` | `ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` |
| `F` | `[{ txid: aaaa…aa, f: 1 }, { txid: bbbb…bb, f: 1000 }]` |
| `|F|` | `2` |
| `draw_won` | `true` |
| `eligible_tx` | both eligible |
| `valid_block` | Yes |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `6` |
| `S_RES(6)` | `10_000` atoms (fixture parent — one prior mint) |

#### Expected Draw Outputs (Kernel)

| Field | Value |
|-------|-------|
| `C` | `499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb` |
| `σ[0]` | `4dbda0def201420293a3777ed7933062700d18c4bab7dc60774b6186f0ac906b` |
| `σ[1]` | `40c32be59db20f79243cf3737cf30db2c7b96fee452eafa3088992a7413ebbfb` |
| `eligible` | `[true, true]` |
| `winner_index` | `1` |
| `winner_txid` | `bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb` |

**Selection rationale:** uint512 cross-multiply prefers index `1` (weight `1000`)
over index `0` (weight `1`) — `weight_priority` regression case.

#### Expected State After

| Field | Value |
|-------|-------|
| `height` | `7` |
| `S_RES(7)` | `20_000` atoms (`200.00 RES`) |
| `M(7)` | `1` |

#### Expected RES Mint Delta

| Field | Atoms |
|-------|-------|
| `res_mint_block` | `10_000` |
| `res_mint_count_block` | `1` |
| `Δ_RES` | `+10_000` |

#### Expected Invariants

| Id | Expected |
|----|----------|
| **CI-030** | **PASS** |
| **CI-031** | **PASS** — exactly one winner despite two eligible |
| **CI-032** | **PASS** (production `T`); fixture `T_draw` for kernel |
| **CI-033** | **PASS** |
| **CI-022** | **PASS** |
| **CI-024** | **PASS** |
| **RES-L1..L7** | **PASS** |

#### Expected Runtime Witness Values (FVR-B / C / F)

| Field | Expected |
|-------|----------|
| `height` | `7` |
| `rs_res_supply_parent` | `10000` |
| `rs_res_supply` | `20000` |
| `cb_res_mint_delta` | `10000` |
| `res_mint_count_M_h` | `1` |

#### Expected Coq Hook Values

| Field | Expected |
|-------|----------|
| `convergence_root_bytes` | `499d0d3b…776fb` (32-byte hex above) |
| `cm_draw_won` | `true` |
| `rs_res_supply` | `20000` |
| `cb_res_mint_delta` | `10000` |
| Kernel winner | index `1` / txid `bbbb…bb` |

#### Expected Acceptance Result

**ACCEPT** — multi-tx draw resolves to single winner; one RES mint.

---

### GV-DRAW-0003 — Draw at Cap Boundary (Informative REJECT)

| Attribute | Value |
|-----------|-------|
| **ID** | `GV-DRAW-0003` |
| **Categories** | GV-004, GV-005, GV-003 (cap) |
| **Status** | **DRAFT** |
| **Description** | Draw kernel may select winner, but block **REJECT** when `S_RES(h−1) = RES_MAX_SUPPLY` |
| **Phase-3 anchor** | Draw inputs from `WIN-002`; cap rule from [GV-RES-0003](./PROOF-002-GOLDEN-RES-VECTORS.md) follow-on |
| **Spec refs** | CI-023, CI-033; CR-045 |

#### Input

| Field | Value |
|-------|-------|
| `height` | `2_100_001` |
| `bind_block_hash` | `bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb` (fixture) |
| `T_draw` | `ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff` |
| `F` | `[{ txid: aaaa…aa, f: 1 }]` (same kernel shape as GV-DRAW-0001) |
| `draw_won` (kernel) | `true` — winner computable at draw layer |
| `eligible_tx` | `aaaa…aa` |
| `block_attempts_res_mint` | `true` |
| `valid_block` | **No** — cap forbids mint |

#### Expected State Before

| Field | Value |
|-------|-------|
| `height` | `2_100_000` |
| `S_RES(2_100_000)` | `2_100_000_000` atoms (`21_000_000.00 RES`) — **at cap** |

#### Expected Draw Outputs (Kernel — isolated)

Draw kernel outputs are **identical in shape** to GV-DRAW-0001 when given the
same `F`, `height`, `bind`, and `T_draw`:

| Field | Value |
|-------|-------|
| `C` | `000…000` |
| `winner_txid` | `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` |
| `eligible[0]` | `true` |

**Separation principle:** Kernel draw comparison (Phase G) may evaluate these
outputs independently of block acceptance. **Block validity** is governed by cap
(CI-023), not by draw kernel alone.

#### Expected State After

| Field | Value |
|-------|-------|
| `S_RES` | **unchanged** — block rejected; no state transition |
| `M(h)` | `0` (no successful mint) |

#### Expected RES Mint Delta

| Field | Value |
|-------|-------|
| `res_mint_block` | `0` |
| `res_mint_count_block` | `0` |
| `Δ_RES` | `0` |

#### Expected Invariants

| Id | Expected |
|----|----------|
| **CI-023** (cap) | **ENFORCED** — rejection prevents `S_RES > RES_MAX_SUPPLY` |
| **CI-033** | **VIOLATED if mint credited** — block must not accept mint at cap |
| **CI-030..CI-032** | Kernel outputs still deterministic (isolated draw check) |
| **CI-022** | **N/A** on reject — no valid transition |
| **RES-L1..L7** | **N/A** on rejected block mirror (no commit) |

#### Expected Runtime Witness Values

No post-connect mirror on rejected block. Pre-reject validation trace may record
draw computation; FVR exports **must not** treat rejected block as authoritative.

| Field | Expected |
|-------|----------|
| Witness capture | **None** for rejected block tip |
| FVR-M `draw_status` | `IMPOSSIBLE` (closure); unrelated to per-block reject |

#### Expected Coq Hook Values

| Field | Expected |
|-------|----------|
| `cm_draw_won` | `true` (kernel input) |
| `cb_res_mint_delta` | `0` (cap guard: `rs_res_supply + reward > MAX`) |
| `apply_connectblock_supply_delta` | No RES increment when cap blocks |

Coq `cb_res_mint_delta` cap guard matches `ConnectBlockMint.v` (`Z.leb` check).

#### Expected Acceptance Result

**REJECT** — `reason: RES_SUPPLY_CAP_REACHED` (CR-045 / CI-023).

Informative rule documented for future vectors:

> When `S_RES(h−1) = RES_MAX_SUPPLY`, any block attempting `M(h) = 1` is
> invalid regardless of draw winner.

---

## 4. Vector Authority

Per [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) §4:

| Rule | Application |
|------|-------------|
| **Hierarchy** | Specification > Vectors > Implementation > Witnesses |
| **Normative** | `ESTABLISHED` vectors fix draw kernel outputs and block outcomes on declared inputs |
| **Witnesses** | FVR-B/C/F corroborate supply fields only; **do not** define draw vectors |
| **FVR-M** | Draw equivalence and closure claims remain **PROHIBITED** regardless of vector match |
| **No back-flow** | Runtime trace MUST NOT amend vector expected values |

**Prohibited:**

- Claiming `coq_equiv_runtime_draw`
- Using FVR-M `IMPOSSIBLE` status to relax CI-030..CI-033 specifications
- Treating kernel `T_draw` fixture as permission to vary production `RES_PROBABILITY_TARGET`

---

## 5. Status Lifecycle

| Vector | PROOF-000 (prior) | This task | Next gate |
|--------|-------------------|-----------|-----------|
| GV-DRAW-0001 | RESERVED | **DRAFT** | Phase G kernel `Compute` vs WIN-002 |
| GV-DRAW-0002 | RESERVED | **DRAFT** | Phase G kernel vs WIN-004 |
| GV-DRAW-0003 | RESERVED | **DRAFT** | Validation harness cap-reject fixture |

**Promotion (DRAFT → ESTABLISHED):**

1. Kernel outputs match `phase3_golden_vectors.json` anchor rows.
2. Block-level outcomes match SPEC-003 / SPEC-002 (ACCEPT or REJECT as declared).
3. FVR supply witnesses match on ACCEPT paths only.
4. FVR-M impossibility acknowledged in promotion checklist.
5. No specification amendment required.

---

## 6. Relationships

### SPEC-001 / SPEC-002 — State and RES coupling

Draw vectors declare `DrawState` inputs (`F`, `C`, σ, winner) and RES supply
before/after. RES recurrence applies only on **ACCEPT** paths (GV-DRAW-0001/0002).

### SPEC-005 — Draw invariants (CI-030..CI-033)

| Invariant | Primary vectors |
|-----------|-----------------|
| CI-030 | All three (kernel determinism) |
| CI-031 | GV-DRAW-0001, GV-DRAW-0002 |
| CI-032 | All (production `T` vs fixture `T_draw` documented) |
| CI-033 | GV-DRAW-0001/0002 PASS; GV-DRAW-0003 reject case |

### FVR-F / FVR-M

| Module | Role |
|--------|------|
| **FVR-F** | Witness chain for RES scalars on ACCEPT paths |
| **FVR-M** | Proves draw closure **IMPOSSIBLE**; vectors do not override |

```text
Vector match on draw kernel  ≠  draw equivalence
Vector match on supply       ≠  global equivalence (equivalence_claimed = false)
```

### Phase-3 golden corpus

| GV-ID | Phase-3 ID | Module |
|-------|----------|--------|
| GV-DRAW-0001 | WIN-002 | RunSelection |
| GV-DRAW-0002 | WIN-004 | RunSelection |
| GV-DRAW-0003 | WIN-002 (kernel) + cap rule | RunSelection + CR-045 |

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
| Draw closure impossibility (FVR-M) | **Preserved** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## Task Output Status

| Field | Status |
|-------|--------|
| Draw Golden Vector Framework | **ESTABLISHED** |
| Vector Entries GV-DRAW | **DRAFT** |
| Proof Infrastructure | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-004` — GV-DRAW-0001..0003 populated (DRAFT); FVR-M boundary documented |
