# PROOF-005: Golden Vector Runtime Verification Harness

**Task:** `TASK-PROOF-006`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** ESTABLISHED (harness specification)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-000](./PROOF-000-GOLDEN-VECTORS.md) … [PROOF-004](./PROOF-004-GOLDEN-STATE-VECTORS.md)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document defines the **Golden Runtime Verification Harness** — the first
offline regression framework that evaluates Golden Consensus Vectors against
Runtime Witness outputs.

```
Golden Vector (normative fixture)
        ↓
Expected State Layer (from PROOF-001..004)
        ↓
Runtime Witness Layer (FVR exports — observational)
        ↓
Comparison Layer (field-by-field diff)
        ↓
Verification Report (GVR_* status)
```

**This task does NOT prove equivalence.**

| Claim | Status |
|-------|--------|
| Coq ≡ Runtime | **NOT ESTABLISHED** |
| Draw equivalence | **PROHIBITED** (FVR-M) |
| **equivalence_claimed** | **MUST remain `false`** |

The harness provides **evidence only** for deterministic regression verification.
It does not modify consensus, validation, state_hash, or FVR layers.

**Implementation note:** This deliverable establishes harness **architecture,
rules, and report schema**. Executable tooling (scripts, CI job) is a future
implementation task; no C++ or FVR code changes in TASK-PROOF-006.

---

## 1. Purpose

### 1.1 Goal

```
Golden Vector  →  Runtime Witness  →  Verification Result
```

For each registered vector `V` and height `h(V)`:

1. Load normative expected fields from PROOF-001..004.
2. Capture observational runtime witness at `h(V)` (when fixture block exists).
3. Compare witness fields to vector expectations under comparison rules (§5).
4. Emit a verification report with `GVR_*` status (§3).

### 1.2 What the harness proves

| Proves | Does not prove |
|--------|----------------|
| Runtime mirror matches vector on fixture inputs | Coq ≡ Runtime |
| FVR-E invariants pass on captured mirror | Global equivalence |
| Witness chain linkage at sampled heights | Draw closure (FVR-M) |
| Regression stability across releases | Specification correctness (vectors are fixtures) |

### 1.3 Binding honesty rules

| Rule | Statement |
|------|-----------|
| **H-001** | `equivalence_claimed` is **always `false`** in every report. |
| **H-002** | Witness **PASS** does not imply CI-* proof. |
| **H-003** | Witness **FAIL** classifies a defect; it does not amend vectors. |
| **H-004** | Vectors remain authoritative over witnesses (PROOF-000 §4). |
| **H-005** | Draw kernel fields may only reach `GVR_PARTIAL` (FVR-M). |

---

## 2. Verification Architecture

### 2.1 Layer diagram

```text
┌─────────────────────────────────────────────────────────────┐
│  Vector Layer (PROOF-000 registry + PROOF-001..004 values) │
│  Status: DRAFT (promotion → ESTABLISHED gates harness CI)   │
└────────────────────────────┬────────────────────────────────┘
                             │ expected fields
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  Expected State Layer                                        │
│  Parsed projection: supply, mint, height, invariants, UTXO  │
└────────────────────────────┬────────────────────────────────┘
                             │ compare()
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  Runtime Witness Layer (FVR — observational, non-authoritative)│
│  RPC / export snapshots at fixture height                    │
└────────────────────────────┬────────────────────────────────┘
                             │ diff
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  Comparison Layer                                            │
│  Category rules (§5); atom-exact numeric equality            │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  Verification Report (§7)                                    │
│  GVR_PASS | GVR_FAIL | GVR_PARTIAL | GVR_NOT_EXECUTED        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Execution modes

| Mode | Description |
|------|-------------|
| **Offline fixture** | Pre-recorded block/connect fixture replayed; witness captured post-connect |
| **Live RPC** | Running node at tip matching fixture height; `getruntimesupply*` at height |
| **Synthetic mirror** | Unit-test injection (`runtime_supply_*_tests`) — dev-only, same comparison rules |

All modes produce the same report schema. Live RPC is preferred for Phase F
promotion (PROOF-000 §9).

### 2.3 Non-participation boundaries

The harness **MUST NOT**:

- Read or write consensus validation state
- Influence block accept/reject
- Update `state_hash` computation
- Modify FVR export code paths
- Set `equivalence_claimed = true`

---

## 3. Verification Result Types

### 3.1 Enumerated statuses

| Status | Code | Definition |
|--------|------|------------|
| **GVR_PASS** | `PASS` | Every **required** checked field matches vector expectation exactly. |
| **GVR_FAIL** | `FAIL` | One or more **required** fields contradict vector expectation. |
| **GVR_PARTIAL** | `PARTIAL` | Witness present but incomplete — optional fields missing, draw blocked (FVR-M), or fixture block not materialized. |
| **GVR_NOT_EXECUTED** | `NOT_EXECUTED` | Vector not yet evaluated (harness skip, missing fixture, or CI not wired). |

### 3.2 Aggregation rules

```
if not executed        → GVR_NOT_EXECUTED
else if no witness     → GVR_PARTIAL  (runtime_witness_present = false)
else if any required fail → GVR_FAIL
else if any optional gap  → GVR_PARTIAL
else                   → GVR_PASS
```

**Required vs optional** per vector category — see §4 field matrices.

### 3.3 Report-level fields (always)

| Field | Value |
|-------|-------|
| `equivalence_claimed` | **`false`** (hard-coded) |
| `coq_equiv_runtime` | **`NOT_ESTABLISHED`** |
| `draw_equivalence` | **`PROHIBITED`** |
| `harness_version` | `proof-005-0.1` |

---

## 4. Verification Targets

**Total: 12 Golden Vectors** (all **DRAFT** until ESTABLISHED promotion).

### 4.1 ORU vectors (PROOF-001)

| Vector ID | Height | Primary witness checks | Expected default |
|-----------|--------|------------------------|------------------|
| **GV-ORU-0001** | `0` | `rs_oru_supply`, `cb_oru_subsidy`, `cb_oru_burn_delta`, ORU-L1..L4 | `GVR_NOT_EXECUTED` until genesis fixture |
| **GV-ORU-0002** | `1` | parent supply, burn `5B` or `10B` atoms per vector, recurrence | `GVR_NOT_EXECUTED` |
| **GV-ORU-0003** | `630000` | `cb_oru_subsidy = 12500000000`, burn delta | `GVR_NOT_EXECUTED` |

### 4.2 RES vectors (PROOF-002)

| Vector ID | Height | Primary witness checks | Expected default |
|-----------|--------|------------------------|------------------|
| **GV-RES-0001** | `10` | `rs_res_supply=10000`, `res_mint_count_M_h=1` | `GVR_NOT_EXECUTED` |
| **GV-RES-0002** | `500` | `rs_res_supply=20000`, parent `10000` | `GVR_NOT_EXECUTED` |
| **GV-RES-0003** | `2100000` | cap boundary `2100000000` | `GVR_NOT_EXECUTED` |

### 4.3 Draw vectors (PROOF-003)

| Vector ID | Height | Primary witness checks | Expected default |
|-----------|--------|------------------------|------------------|
| **GV-DRAW-0001** | `1` | supply coupling; draw kernel fields **optional** | `GVR_PARTIAL` (draw CES blocked) |
| **GV-DRAW-0002** | `7` | supply coupling; winner txid **optional** | `GVR_PARTIAL` |
| **GV-DRAW-0003** | `2100001` | REJECT path — **no post-connect witness** | `GVR_PARTIAL` or `GVR_NOT_EXECUTED` |

Draw vectors: maximum achievable status is **`GVR_PARTIAL`** for draw-specific
fields under FVR-M; supply scalar checks may still **PASS**.

### 4.4 State vectors (PROOF-004)

| Vector ID | Height | Primary witness checks | Expected default |
|-----------|--------|------------------------|------------------|
| **GV-STATE-0001** | `0` | full supply + height + FVR-F genesis witness | `GVR_NOT_EXECUTED` |
| **GV-STATE-0002** | `10` | composes ORU+RES+draw supply; UTXO cardinality **optional** | `GVR_NOT_EXECUTED` |
| **GV-STATE-0003** | `6` (reorg) | post-reorg `rs_oru_supply`, `rs_res_supply`; FVR-F rewind | `GVR_NOT_EXECUTED` |

### 4.5 Target registry summary

| Family | IDs | Count |
|--------|-----|-------|
| ORU | GV-ORU-0001..0003 | 3 |
| RES | GV-RES-0001..0003 | 3 |
| Draw | GV-DRAW-0001..0003 | 3 |
| State | GV-STATE-0001..0003 | 3 |
| **Total** | | **12** |

---

## 5. Comparison Rules

### 5.1 Categories

| Category | Witness fields | Comparison | Required |
|----------|----------------|------------|----------|
| **ORU supply** | `oru_supply_cumulative`, `rs_oru_supply` | Atom-exact `==` | Yes (ORU/STATE vectors) |
| **ORU subsidy** | `oru_subsidy_block`, `cb_oru_subsidy` | Atom-exact `==` | Yes |
| **ORU burn** | `oru_converge_burn_block`, `cb_oru_burn_delta` | Atom-exact `==` | Yes when vector declares `C(h)>0` |
| **RES supply** | `res_mint_cumulative`, `rs_res_supply` | Atom-exact `==` | Yes (RES/STATE vectors) |
| **RES mint delta** | `res_mint_block`, `cb_res_mint_delta` | Atom-exact `==` | Yes when `M(h)=1` |
| **Mint count** | `res_mint_count_block`, `res_mint_count_M_h` | Integer `==` | Yes (RES/DRAW/STATE) |
| **Height** | `height` | Integer `==` | Yes |
| **Parent linkage** | `oru_supply_parent`, `rs_res_supply_parent`, `parent_hash` | Atom-exact / hash `==` | Yes when vector declares parent |
| **UTXO cardinality** | `utxo_res_balance` diagnostic | Optional count bound | Optional (STATE vectors) |
| **Draw winner** | kernel / RPC draw export | Hex txid `==` | **Optional** — max `GVR_PARTIAL` |
| **Runtime invariants** | FVR-E `passed`, `violations` | `passed=true`, `violations=[]` | Yes when witness captured |
| **FVR witness status** | FVR-F chain valid, catalog row present | Boolean flags | Optional |

### 5.2 Comparison primitives

```
compare_atoms(expected, witness) :=
  if witness missing     → OPTIONAL_MISS
  if expected == witness → MATCH
  else                   → MISMATCH

compare_required(field) :=
  MISMATCH → counts toward GVR_FAIL
  OPTIONAL_MISS on required field → GVR_FAIL

compare_optional(field) :=
  MISMATCH → note only; may yield GVR_PARTIAL
  OPTIONAL_MISS → GVR_PARTIAL
```

**Numeric format:** All amounts in **base atoms** (ORU: `COIN=1e8`; RES: `RES_COIN=100`).

### 5.3 Prohibited comparison actions

| Prohibited | Reason |
|------------|--------|
| Rewriting vector expected values from witness | PROOF-000 §4 V-005 |
| Treating `GVR_PASS` as equivalence | H-001 |
| Using witness to fix consensus caches | Constitutional rule 2 |
| Closing FVR-M via harness pass | Draw impossibility proven |

### 5.4 Per-vector required field sets (minimal)

| Vector | Required fields |
|--------|-----------------|
| GV-ORU-* | `height`, `rs_oru_supply`, `cb_oru_subsidy`, `cb_oru_burn_delta`, `rs_oru_supply_parent`, FVR-E pass |
| GV-RES-* | `height`, `rs_res_supply`, `cb_res_mint_delta`, `res_mint_count_M_h`, FVR-E pass |
| GV-DRAW-* | RES supply fields when mint declared; draw winner **optional** |
| GV-STATE-* | All required fields from composed ORU+RES rows at vector height |

---

## 6. Runtime Witness Sources

Runtime witnesses are **evidence only**. Witness disagreement does not redefine
vectors.

### 6.1 FVR layer map

| Layer | Artifact | RPC / export | Harness use |
|-------|----------|--------------|-------------|
| **FVR-B** | `RuntimeSupplyState` mirror | `getruntimesupply`, `exportruntimesupply` | Primary supply scalars |
| **FVR-C** | `CoqSupplyFieldHook` | `coq_fields` nested in export | Canonical comparison names |
| **FVR-F** | Witness chain node | `getruntimesupplywitness`, `getruntimesupplywitnessbyheight` | Parent linkage, history |
| **FVR-G** | Coq catalog row | `getruntimesupplywitnesscatalog` | Row presence audit |
| **FVR-H** | CHAS alignment | `getphase5runtimealignment` | Alignment evidence (non-pass/fail for equiv) |
| **FVR-I** | Candidate equivalence | embedded in catalog / alignment | CES classification only |
| **FVR-K** | Global candidate index | alignment report composite | Observational |
| **FVR-N** | Phase-5 summary | `getphase5summary` | Honesty boundary audit (`equivalence_claimed=false`) |

**Supporting RPC (non-equivalence):**

| RPC | Role |
|-----|------|
| `getruntimesupplyinvariants` | FVR-E ORU-L / RES-L checks |
| `getdrawclosurestatus` | FVR-M impossibility confirmation |

### 6.2 Witness capture procedure (normative)

1. Ensure fixture block at `height` is connected (functional test or regtest).
2. Call `getruntimesupply <height>` → extract mirror + `coq_fields`.
3. Call `getruntimesupplyinvariants <height>` → FVR-E result.
4. Call `getruntimesupplywitnessbyheight <height>` → FVR-F linkage.
5. Optional: `getruntimesupplywitnesscatalog`, `getphase5runtimealignment`, `getphase5summary`.
6. Run comparison layer; emit report.

**No step modifies runtime state.**

### 6.3 Witness absence handling

| Condition | Status |
|-----------|--------|
| Node not running | `GVR_NOT_EXECUTED` |
| Height not on chain | `GVR_PARTIAL` |
| Rejected block (GV-DRAW-0003) | `GVR_PARTIAL` — no mirror expected |
| Draw-only fields | `GVR_PARTIAL` — FVR-M blocked |

---

## 7. Verification Report Format

### 7.1 Canonical single-vector report

```json
{
  "harness_version": "proof-005-0.1",
  "vector_id": "GV-ORU-0001",
  "vector_status": "DRAFT",
  "verification_status": "GVR_NOT_EXECUTED",
  "mismatch_count": 0,
  "checked_fields": 0,
  "failed_fields": [],
  "runtime_witness_present": false,
  "equivalence_claimed": false,
  "coq_equiv_runtime": "NOT_ESTABLISHED",
  "draw_equivalence": "PROHIBITED",
  "field_results": [],
  "notes": []
}
```

### 7.2 Field result entry

```json
{
  "category": "oru_supply",
  "field": "rs_oru_supply",
  "expected": "100000000000",
  "witness": "100000000000",
  "result": "MATCH",
  "source": "FVR-C",
  "required": true
}
```

**`result` values:** `MATCH`, `MISMATCH`, `OPTIONAL_MISS`, `SKIPPED`.

### 7.3 Batch report (12 vectors)

```json
{
  "harness_version": "proof-005-0.1",
  "run_id": "<uuid>",
  "baseline": "mainnet-freeze-rfc015-2026-05-26",
  "equivalence_claimed": false,
  "summary": {
    "total": 12,
    "pass": 0,
    "fail": 0,
    "partial": 0,
    "not_executed": 12
  },
  "vectors": [ "...12 single-vector reports..." ],
  "fvr_n_honesty": {
    "source": "getphase5summary",
    "equivalence_claimed": false,
    "draw_status": "IMPOSSIBLE"
  }
}
```

### 7.4 Required top-level keys

| Key | Type | Description |
|-----|------|-------------|
| `vector_id` | string | e.g. `GV-ORU-0001` |
| `verification_status` | string | `GVR_PASS` \| `GVR_FAIL` \| `GVR_PARTIAL` \| `GVR_NOT_EXECUTED` |
| `mismatch_count` | int | Count of `MISMATCH` on required fields |
| `checked_fields` | int | Total fields compared |
| `failed_fields` | string[] | Names of required fields with `MISMATCH` |
| `runtime_witness_present` | bool | Whether FVR-B export was obtained |
| `notes` | string[] | Human-readable gaps (FVR-M, fixture missing, etc.) |

---

## 8. Future Coq Integration

### 8.1 Planned chain (not implemented)

```text
Golden Vector
        ↓
Runtime Verification Harness (this document)     ← ESTABLISHED (spec)
        ↓
Coq Replay / Compute harness                     ← NOT IMPLEMENTED
        ↓
Candidate Equivalence (FVR-I/J/K)                ← ACTIVE (classification only)
        ↓
Scoped equivalence proof                         ← NOT ESTABLISHED
```

### 8.2 Explicit non-claims

| Item | Status |
|------|--------|
| Coq `Compute` per-vector runner | **NOT IMPLEMENTED** |
| `coq_equiv_runtime` theorem | **NOT ESTABLISHED** |
| PD-4F-OB-02 closure via harness | **PROHIBITED** |
| Draw refinement proof | **PROHIBITED** (FVR-M) |

### 8.3 Future harness extensions (documentation only)

1. **Phase G:** Add `coq_field_results[]` parallel to `field_results[]`.
2. **Tri-layer diff:** Vector vs Runtime vs Coq catalog row (FVR-G).
3. **Promotion gate:** All 12 vectors `GVR_PASS` on supply fields → vector `DRAFT→ESTABLISHED` review (draw remains `PARTIAL`).

---

## 9. Freeze Compatibility

| Guarantee | Status |
|-----------|--------|
| No protocol modifications | **Confirmed** |
| No consensus modifications | **Confirmed** |
| No validation modifications | **Confirmed** |
| No state_hash modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No runtime witness (FVR) modifications | **Confirmed** |
| FVR-M draw impossibility preserved | **Confirmed** |

This document specifies harness behavior only. Existing RPC methods are
**referenced**, not altered.

**Suggested manual verification (when fixtures exist):**

```bash
cd ./core
# After regtest/fixture node at required height:
./src/rigor-cli getruntimesupply <height>
./src/rigor-cli getruntimesupplyinvariants <height>
./src/rigor-cli getruntimesupplywitnessbyheight <height>
./src/rigor-cli getphase5summary
./src/rigor-cli getdrawclosurestatus
```

Compare output to PROOF-001..004 expected witness tables manually until
executable harness script lands in a future task.

---

## Task Output Status

| Field | Status |
|-------|--------|
| Runtime Vector Harness | **ESTABLISHED** |
| Verification Categories | **ESTABLISHED** |
| Verification Result Types | **ESTABLISHED** |
| Golden Vector Targets | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-006` — runtime vector harness architecture, rules, report schema |
