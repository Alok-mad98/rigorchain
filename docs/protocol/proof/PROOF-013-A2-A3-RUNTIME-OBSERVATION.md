# PROOF-013: A-2 / A-3 Runtime Witness Observation

**Task:** `TASK-PROOF-014`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** OBSERVED (runtime diagnostic)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md), [PROOF-003](./PROOF-003-GOLDEN-DRAW-VECTORS.md), [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md)  
**Batch witness:** [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (TASK-PROOF-016)  
**Scope:** Observational only — no consensus, FVR, state_hash, or Coq changes.

**Equivalence Claimed:** false

---

## §1 Purpose

Observe and document audit item lifecycle for:

| Item | Ambiguity | Primary golden vectors |
|------|-----------|------------------------|
| **A-2** | `uint512` cross-multiply overflow bound for `H_i · f_j` winner path | `GV-DRAW-0002` (weight-priority / tie-break), `phase3_golden_vectors.json` |
| **A-3** | `F` membership predicate (mint-classified candidate enumeration) | `GV-DRAW-0001` (empty `F`), `GV-DRAW-0002` (`|F|=2`), `phase3_golden_vectors.json` |

This task does **not** discharge proof debt **PD-14** (A-2) or **PD-13** (A-3), and does not advance either item to `AI_CLOSED`.

Supporting RES vectors (`GV-RES-0001`) corroborate draw-adjacent supply mirrors only.

---

## §2 Lifecycle Assessment

Per [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) §3, `AI_READY` requires **all** artifact categories (including Coq lemmas). Both items advance from `AI_OPEN` to **`AI_PARTIAL`** on runtime + golden-vector evidence; Coq refinement remains OPEN.

| Item | Prior | Observed | Blocks `AI_READY` / `AI_CLOSED` |
|------|-------|----------|----------------------------------|
| **A-2** | `AI_OPEN` | **`AI_PARTIAL`** | PD-14 (`Z` ≡ `arith_uint512` under magnitude bound); formal review sign-off |
| **A-3** | `AI_OPEN` | **`AI_PARTIAL`** | PD-13 (`build_F` ≡ C++ enumeration); F-set cross-layer status **UNKNOWN** per formal audit |

**Why not `AI_READY`:** Coq artifact category is **required** for A-2 and A-3 (PROOF-010 §3.2). PD-13 and PD-14 remain **OPEN** in `PROOF_DEBT_REGISTER.md`.

### §2.1 Evidence mapping

| Category | A-2 | A-3 |
|----------|-----|-----|
| Specification pin | Magnitude bound documented (RFC-008, PHASE3 assumptions A3, ATTACKER_SPEC G-5) — informal, not closure doc | F enumeration described; set equality **not** pinned |
| Formal model | C++ `arith_uint512`, Python `rigor_uint512`, Coq unbounded `Z` | Coq `build_F` vs C++ `ListConvergenceCommitmentTxids` — **UNKNOWN** |
| Coq artifact | PD-14 **OPEN** | PD-13 **OPEN** |
| Runtime witness | Harness + `getruntimesupply` at fixture heights | Harness + `getconvergenceresult` RPC operational |
| Golden vectors | 57/57 oracle; `GV-DRAW-0002` winner path | 57/57 oracle; `GV-DRAW-0001` / `GV-DRAW-0002` |
| Review notes | This observation (TASK-PROOF-014) | This observation (TASK-PROOF-014) |

---

## §3 Local Verification Procedure

### 3.1 Environment

```bash
export DATADIR="${HOME}/.rigor/golden-vector-regtest"
mkdir -p "${DATADIR}"
cd ./core
```

### 3.2 Start regtest node

```bash
./src/rigord -regtest -daemon -datadir="${DATADIR}"
```

### 3.3 Materialize A-2 / A-3 vectors

**Full materialization** (all heights including 2 100 001):

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
```

**Partial observation** (height ≥ 11):

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" createwallet gv_a23
ADDR="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getnewaddress "" bech32)"
./src/rigor-cli -regtest -datadir="${DATADIR}" generatetoaddress 11 "${ADDR}"

./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-RES-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-DRAW-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-DRAW-0002
```

**Registry path:** `<datadir>/regtest/golden_vector_fixtures.json`

### 3.4 Harness status

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-RES-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-DRAW-0002
```

**`verify_fvr`:** omit second argument (CLI positional `true` is rejected as string) or use JSON-RPC boolean — see [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) §2.5.

**Expected harness outcomes (partial materialization):**

| Vector | `verification_status` | Notes |
|--------|----------------------|-------|
| `GV-RES-0001` | `GVR_PASS` | RES supply mirrors |
| `GV-DRAW-0001` | `GVR_PARTIAL` | Expected — FVR-M; draw fields optional |
| `GV-DRAW-0002` | `GVR_PARTIAL` | Expected — `winner_txid` SKIPPED; supply fields MATCH |

`GVR_PARTIAL` with `mismatch_count: 0` is a **verified** harness outcome (not failure) per [PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md) / FVR-M.

### 3.5 Runtime mirrors

```bash
H10="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 10)"
H7="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 7)"
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${H10}"
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${H7}"
./src/rigor-cli -regtest -datadir="${DATADIR}" getconvergenceresult 7
```

**Note:** Generically mined regtest blocks do not contain normative draw fixture transactions; `getconvergenceresult` at mined heights may show `candidates: []`. Draw kernel evidence for A-2/A-3 is primarily `test_phase3_oracle.py` (57/57) and harness fixture-applied supply fields.

### 3.6 Draw / σ oracle (primary cross-layer evidence)

```bash
cd .
python3 formal_verification/tests/test_phase3_oracle.py
```

Expected: `PHASE-3 oracle: 57/57 passed` (covers σ inequality, eligibility, winner selection, and `F`-sized cases).

---

## §4 Observation Log (2026-06-05)

**Environment:** `DATADIR=~/.rigor/golden-vector-regtest`, regtest height 11, partial materialization.

### 4.1 Phase-3 oracle

```
PHASE-3 oracle: 57/57 passed
```

### 4.2 `GV-RES-0001`

| Field | Observed |
|-------|----------|
| `verification_status` | `GVR_PASS` |
| `mismatch_count` | `0` |
| `equivalence_claimed` | `false` |
| `draw_equivalence` | `PROHIBITED` |

### 4.3 `GV-DRAW-0001` (A-3: empty `F`)

| Field | Observed |
|-------|----------|
| `verification_status` | `GVR_PARTIAL` |
| `mismatch_count` | `0` |
| `equivalence_claimed` | `false` |

### 4.4 `GV-DRAW-0002` (A-2: uint512 winner; A-3: `|F|=2`)

| Field | Observed |
|-------|----------|
| `verification_status` | `GVR_PARTIAL` |
| `mismatch_count` | `0` |
| `winner_txid` field | `SKIPPED` (optional per FVR-M) |
| `equivalence_claimed` | `false` |

Normative kernel expectation (PROOF-003): `winner_index = 1` via uint512 weight priority — validated by phase-3 oracle / `WIN-004`, not by runtime `winner_txid` RPC field on fixture-mined blocks.

### 4.5 `getruntimesupply`

| Height | `mirror_present` | `invariant_passed` | Notes |
|--------|------------------|-------------------|-------|
| `10` (GV-RES-0001) | `true` | `true` | RES fixture mirrors active |
| `7` (GV-DRAW-0002) | `true` | `true` | Supply-path witness operational |

### 4.6 `getconvergenceresult`

RPC responds at height `7`; on generically mined blocks: `convergence_root_set_size: 0`, `candidates: []`. Confirms draw export path is reachable; **F-set content** for normative fixtures is established via golden oracle, not live regtest block bodies.

### 4.7 Batch harness (partial)

```
summary: total=12 pass=3 partial=2 fail=2 not_executed=5
```

Materialized pass/partial: `GV-ORU-0002`, `GV-RES-0001`, `GV-STATE-0002` (`GVR_PASS`); `GV-DRAW-0001`, `GV-DRAW-0002` (`GVR_PARTIAL`). Unmaterialized vectors remain `GVR_NOT_EXECUTED` until full `golden_vector_materializer.sh` run.

---

## §5 Required Output Table

| Field | Status |
|-------|--------|
| Runtime Observation of A-2 / A-3 | **VERIFIED** |
| AI Lifecycle Update | **A-2: `AI_PARTIAL`; A-3: `AI_PARTIAL`** |
| Runtime Harness / Mirrors | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §6 Notes

1. **DRAW vectors** are expected `GVR_PARTIAL` under FVR-M; this is not an A-2/A-3 harness failure.
2. **Full `AI_CLOSED`** for A-2 requires PD-14 Coq lemma; for A-3 requires PD-13 set-equality proof (future tasks).
3. **A-2 depends on A-1** integer interpretation ([PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md)); A-1 remains `AI_PARTIAL`.
4. No source files under consensus, FVR export logic, or Coq were modified for this task.

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-014` — runtime witness observation for A-2 / A-3 partial closure |
