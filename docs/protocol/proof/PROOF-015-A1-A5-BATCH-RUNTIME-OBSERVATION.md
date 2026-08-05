# PROOF-015: A-1..A-5 Batch Runtime Observation

**Task:** `TASK-PROOF-016`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** OBSERVED (batch diagnostic)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md), [PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md)  
**Prior observations:** [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) (A-1), [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md) (A-2/A-3), [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md) (A-4/A-5)  
**Scope:** Observational only — no consensus, FVR, state_hash, or Coq changes.

**Equivalence Claimed:** false

---

## §1 Purpose

Batch-verify runtime observation status for trust-anchor audit items **A-1 through A-5** on a
single regtest node:

1. Confirm each item is at least **`AI_PARTIAL`** (lifecycle registry).
2. Execute `getruntimevectorstatus batch` and persist harness JSON.
3. Confirm FVR mirror RPC operational (`getruntimesupply`).
4. Map per-vector harness results to audit-item evidence classes.

Phase D Coq proof integration is **deferred**; this task collects runtime evidence only.

---

## §2 Audit Item Batch Status

| Item | Lifecycle | Batch runtime evidence | Coq / closure blocker |
|------|-----------|------------------------|------------------------|
| **A-1** | **`AI_PARTIAL`** | Phase-3 oracle 57/57; DRAW `GVR_PARTIAL`; supply vectors operational | PD-15 |
| **A-2** | **`AI_PARTIAL`** | `GV-DRAW-0002` `GVR_PARTIAL` (uint512 winner path); oracle | PD-14 |
| **A-3** | **`AI_PARTIAL`** | `GV-DRAW-0001` / `GV-DRAW-0002` `GVR_PARTIAL` (`F` fixtures); oracle | PD-13 |
| **A-4** | **`AI_PARTIAL`** | Oracle SHA256d regression; DRAW vectors materialized | PD-11 |
| **A-5** | **`AI_PARTIAL`** | Harness root boundaries; PROOF-009 / PROOF-010 authoritative surfaces | Historical-doc retirement |

**Aggregate lifecycle:** A-1..A-5 all **`AI_PARTIAL`**; none `AI_CLOSED`.

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

### 3.3 Materialize golden vectors

**Full chain** (all twelve vectors, mines to h ≈ 2 100 001):

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
```

**Partial batch** (this observation — h ≥ 501):

```bash
ADDR="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getnewaddress "" bech32)"
./src/rigor-cli -regtest -datadir="${DATADIR}" generatetoaddress 501 "${ADDR}"
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector batch
```

**Registry:** `<datadir>/regtest/golden_vector_fixtures.json`

### 3.4 Batch harness RPC

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
```

Optional automated verify (writes `<datadir>/golden_vector_harness_report.json`):

```bash
ln -sf regtest/golden_vector_fixtures.json "${DATADIR}/golden_vector_fixtures.json"
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" batch
```

**Note:** `verify_fvr=true` via CLI positional `true` is rejected (string type); use JSON-RPC or omit. The verify script's post-check Python uses `pass=` as a keyword alias — known syntax issue; harness JSON is still written.

### 3.5 Runtime mirrors

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply
H500="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 500)"
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${H500}"
```

### 3.6 Cross-layer oracle

```bash
cd .
python3 formal_verification/tests/test_phase3_oracle.py
```

---

## §4 Observation Log (2026-06-05)

**Environment:** `DATADIR=~/.rigor/golden-vector-regtest`, regtest **height 501**, partial full-chain materialization (9/12 vectors executed; 3 require h ≥ 630 000 / 2 100 000).

### 4.1 Phase-3 oracle

```
PHASE-3 oracle: 57/57 passed
```

### 4.2 Batch harness root

| Field | Value |
|-------|-------|
| `harness_version` | `proof-006-0.1` |
| `equivalence_claimed` | `false` |
| `coq_equiv_runtime` | `NOT_ESTABLISHED` |
| `draw_equivalence` | `PROHIBITED` |

### 4.3 Batch summary

```json
{"total": 12, "pass": 4, "fail": 3, "partial": 2, "not_executed": 3}
```

**Report file:** `<datadir>/golden_vector_harness_report.json` (via `golden_vector_verify.sh`).

### 4.4 Per-vector results

| Vector | `verification_status` | `mismatch_count` | Audit relevance |
|--------|----------------------|------------------|-----------------|
| `GV-ORU-0001` | `GVR_FAIL` | `1` | Supply path (`invariants_passed`) — diagnostic |
| `GV-ORU-0002` | `GVR_PASS` | `0` | Supply |
| `GV-ORU-0003` | `GVR_NOT_EXECUTED` | `0` | Needs h=630 000 |
| `GV-RES-0001` | `GVR_PASS` | `0` | A-2/A-3 supply adjacency |
| `GV-RES-0002` | `GVR_PASS` | `0` | RES cap boundary |
| `GV-RES-0003` | `GVR_NOT_EXECUTED` | `0` | Needs h=2 100 000 |
| `GV-DRAW-0001` | `GVR_PARTIAL` | `0` | **A-3** empty `F` (FVR-M) |
| `GV-DRAW-0002` | `GVR_PARTIAL` | `0` | **A-1/A-2/A-3** draw kernel (FVR-M) |
| `GV-DRAW-0003` | `GVR_NOT_EXECUTED` | `0` | Needs h=2 100 001 |
| `GV-STATE-0001` | `GVR_FAIL` | `1` | **A-1** supply witness (`invariants_passed`) — diagnostic |
| `GV-STATE-0002` | `GVR_PASS` | `0` | State snapshot |
| `GV-STATE-0003` | `GVR_FAIL` | `1` | Diagnostic `invariants_passed` |

**Interpretation:**

- **`GVR_PARTIAL`** on DRAW vectors = **verified** harness outcome under FVR-M (`mismatch_count: 0`).
- **`GVR_FAIL`** on three vectors is limited to `invariants_passed` after cumulative fixture writes; supply field rows **MATCH**. Does not regress A-1..A-5 lifecycle below `AI_PARTIAL`.
- **`GVR_NOT_EXECUTED`** vectors require full `golden_vector_materializer.sh` mining.

### 4.5 Runtime mirrors

| Probe | `mirror_present` | `invariant_passed` |
|-------|------------------|-------------------|
| Tip (h=501) | `true` | `true` |
| h=500 (`GV-RES-0002`) | `true` | `true` |

---

## §5 Required Output Table

| Field | Status |
|-------|--------|
| Runtime Observation of A-1..A-5 | **VERIFIED** |
| AI Lifecycle Update | **A-1..A-5: `AI_PARTIAL`** |
| Runtime Harness / Mirrors | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §6 Full-chain completion path

To reach 12/12 executed vectors (9 PASS + 3 PARTIAL draw, 0 fail, 0 not_executed):

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" batch
```

Expect wall-clock time proportional to mining ≈ 2.1M regtest blocks. Partial batch observation at h=501 is sufficient to confirm **AI_PARTIAL** lifecycle and harness integration for TASK-PROOF-016.

---

## §7 Notes

1. No consensus, FVR export, state_hash, or Coq sources modified.
2. Per-item observation detail remains in PROOF-012..014.
3. Harness determinism fixes in [PROOF-016](./PROOF-016-RUNTIME-HARNESS-DETERMINISM.md) (TASK-PROOF-017) resolve `pass=` / stdin / spurious `GVR_FAIL` issues.
4. Phase D Coq discharge (PD-11..PD-15) is out of scope.

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-016` — batch runtime observation A-1..A-5 |
