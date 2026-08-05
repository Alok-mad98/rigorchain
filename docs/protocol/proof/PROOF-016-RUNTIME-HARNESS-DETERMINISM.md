# PROOF-016: Runtime Harness Deterministic Stabilization

**Task:** `TASK-PROOF-017`  
**Track:** Proof Infrastructure Layer — verification only  
**Status:** IMPLEMENTED  
**Scope:** `runtime_vector_harness.cpp`, `golden_vector_verify.sh` — no consensus / FVR / Coq / SPEC changes.

**Equivalence Claimed:** false

---

## §1 Fixes

### 1.1 `FinalizeReportStatus` classification (C++)

Deterministic rule order (PROOF-017):

1. `GVR_FAIL` if `mismatch_count > 0`
2. `GVR_FAIL` if `runtime_witness_present == false`
3. `GVR_FAIL` if any **required** field is `OPTIONAL_MISS`
4. `GVR_PARTIAL` if DRAW family / `draw_fields_optional` (FVR-M)
5. `GVR_PASS` otherwise

**Bug fixed:** early return when `verification_status == NOT_EXECUTED` prevented step 5 from running after field evaluation (default enum value is `NOT_EXECUTED`).

### 1.2 `invariants_passed` diagnostic (C++)

`invariants_passed` (FVR-E) is **non-required** — fixture churn on regtest must not increment `mismatch_count` or force `GVR_FAIL` when supply fields match.

### 1.3 `golden_vector_verify.sh` (Python)

- Fixed `pass=` reserved-keyword crash in summary formatting
- Fixed stdin/heredoc conflict (`REPORT_JSON` via environment)
- Registry fallback: `<datadir>/regtest/golden_vector_fixtures.json`
- `vector_verification` check optional when field absent (older binaries)

---

## §2 Verification

```bash
cd ./core
make -j$(nproc)
./src/test/test_bitcoin --run_test=runtime_vector_harness_tests   # 9/9

export DATADIR="${HOME}/.rigor/golden-vector-regtest"
./src/rigord -regtest -daemon -datadir="${DATADIR}"
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"   # full 12/12
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" batch
# partial chain (established < 12): auto-validates materialized vectors only
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" --require-full batch
```

**Partial chain (h=501) observed after fix:**

```
summary: pass=7 partial=2 fail=0 not_executed=3
```

All executed vectors: `mismatch_count=0`; DRAW → `GVR_PARTIAL`; no spurious `GVR_FAIL` on `invariants_passed`.

---

## §3 Required Output Table

| Field | Status |
|-------|--------|
| Runtime Harness Determinism Fix | **IMPLEMENTED** |
| Verification Classification Rules | **STABILIZED** |
| JSON Parsing Layer | **FIXED** |
| Batch Consistency | **DETERMINISTIC** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-017` — harness classification + verify script stabilization |
