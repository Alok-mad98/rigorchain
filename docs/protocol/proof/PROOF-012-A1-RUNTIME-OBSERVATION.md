# PROOF-012: A-1 Runtime Witness Observation

**Task:** `TASK-PROOF-013`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** OBSERVED (runtime diagnostic)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md), [PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md)  
**Related:** [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md) (A-2 / A-3), [PROOF-014](./PROOF-014-A4-A5-RUNTIME-OBSERVATION.md) (A-4 / A-5), [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (A-1..A-5 batch, TASK-PROOF-016)  
**Scope:** Observational only — no consensus, FVR, state_hash, or Coq changes.

**Equivalence Claimed:** false

---

## §1 Purpose

Confirm audit item **A-1** (σ byte order) remains **`AI_PARTIAL`** while the runtime golden-vector
harness and FVR supply mirrors are operational on regtest. This task does **not** discharge proof
debt **PD-15** or advance A-1 to `AI_CLOSED`.

A-1 runtime evidence is **indirect**:

| Path | Role |
|------|------|
| `phase3_golden_vectors.json` + `test_phase3_oracle.py` | Direct σ / inequality cross-layer (57 vectors) |
| `GV-STATE-0001` harness + `getruntimesupply` | Supply-path witness pipeline (not σ-specific) |
| PROOF-011 §3.1 LE integer pin | Normative specification anchor |

---

## §2 Local Verification Procedure

### 2.1 Prerequisites

```bash
cd ./core
# User builds locally (full make not run by agent):
# make -j$(nproc)
```

### 2.2 Environment

```bash
export DATADIR="${HOME}/.rigor/golden-vector-regtest"
mkdir -p "${DATADIR}"
```

### 2.3 Start regtest node

```bash
./src/rigord -regtest -daemon -datadir="${DATADIR}"
```

### 2.4 Materialize fixtures

**Full chain (all twelve vectors):** mines to height 2 100 001 — slow; see
[PROOF-007](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md).

**Partial observation (A-1 supporting, low height):** sufficient for `GV-STATE-0001` at genesis:

```bash
# Wallet + mine to height ≥ 11 (covers GV-RES-0001 @ h=10, GV-STATE-0002 @ h=10)
./src/rigor-cli -regtest -datadir="${DATADIR}" createwallet gv_obs
ADDR="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getnewaddress "" bech32)"
./src/rigor-cli -regtest -datadir="${DATADIR}" generatetoaddress 11 "${ADDR}"

# Materialize A-1 supporting supply vectors
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-STATE-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-STATE-0002   # optional
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-RES-0001    # optional
```

Or use the batch materializer script when full height is available:

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
```

**Registry path:** with `-regtest -datadir="${DATADIR}"`, fixtures are stored at
`<datadir>/regtest/golden_vector_fixtures.json` (not the datadir root).

### 2.5 Harness status (RPC)

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-STATE-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
```

**`verify_fvr` note:** the second parameter is JSON **boolean**. `rigor-cli` positional `true` is
passed as a string and RPC rejects it (`Wrong type passed`). Omit the second argument (default
`verify_fvr=false`) or use JSON-RPC: `"params": ["GV-STATE-0001", true]`.

**PASS criteria for GV-STATE-0001:**

| Field | Expected |
|-------|----------|
| `vector_status` | `ESTABLISHED` |
| `verification_status` | `GVR_PASS` |
| `mismatch_count` | `0` |
| `runtime_witness_present` | `true` |
| `equivalence_claimed` | `false` |
| `coq_equiv_runtime` | `NOT_ESTABLISHED` |
| `draw_equivalence` | `PROHIBITED` |

When built from current sources, harness JSON also includes `vector_verification: VERIFIED` for
`GVR_PASS` vectors ([PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md)). Semantic equivalence:
`GVR_PASS` + `mismatch_count: 0` + `vector_status: ESTABLISHED` ⇒ verified outcome.

### 2.6 Runtime supply mirror

```bash
GENHASH="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 0)"
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${GENHASH}"
# Or tip:
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply
```

### 2.7 σ draw vector tests (primary A-1 evidence)

```bash
cd .
python3 formal_verification/tests/test_phase3_oracle.py
```

Expected: `PHASE-3 oracle: 57/57 passed`.

### 2.8 Full batch verification (optional)

Requires all twelve vectors materialized and registry symlink for verify script:

```bash
ln -sf regtest/golden_vector_fixtures.json "${DATADIR}/golden_vector_fixtures.json"
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}"
```

---

## §3 Observation Log (2026-06-05)

**Environment:** WSL2, `DATADIR=~/.rigor/golden-vector-regtest`, regtest height 11 after
`generatetoaddress`, partial materialization (not full 2.1M chain).

### 3.1 σ oracle (direct A-1 layer agreement)

```
PHASE-3 oracle: 57/57 passed
```

### 3.2 `materializegoldenvector GV-STATE-0001`

| Field | Observed |
|-------|----------|
| `vector_id` | `GV-STATE-0001` |
| `vector_status` | `ESTABLISHED` |
| `verification_status` | `GVR_PASS` |
| `mismatch_count` | `0` |
| `checked_fields` | `10` |
| `runtime_witness_present` | `true` |
| `equivalence_claimed` | `false` |

All required field_results: `MATCH` (height 0, ORU/RES supply, invariants).

### 3.3 `getruntimevectorstatus GV-STATE-0001`

Same as §3.2 after re-materialization. Intermediate batch view may show `GVR_FAIL` on
`GV-STATE-0001` until re-materialized after other fixture writes — re-run
`materializegoldenvector GV-STATE-0001` to restore.

### 3.4 `getruntimesupply` (genesis block)

| Field | Observed |
|-------|----------|
| `height` | `0` |
| `mirror_present` | `true` |
| `invariant_passed` | `true` |
| `oru_subsidy_block` | `1000.0` |
| `mirror_source` | `stored_mirror` |

### 3.5 Audit item A-1 lifecycle

Per [PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md): **`AI_PARTIAL`** unchanged.

| Closure gate | Status |
|--------------|--------|
| Spec LE pin (§3.1) | SATISFIED |
| Cross-layer σ oracle (57/57) | SATISFIED |
| Runtime harness / mirrors | SATISFIED (observed) |
| Coq refinement PD-15 | **OPEN** → blocks `AI_CLOSED` |

---

## §4 Required Output Table

| Field | Status |
|-------|--------|
| Runtime Observation of A-1 | **VERIFIED** |
| AI Lifecycle Update | **`AI_PARTIAL`** |
| Runtime Harness / Mirrors | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §5 Notes

1. **GV-STATE vectors** do not compare σ integers; they corroborate FVR export integrity only
   ([PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md) §6.1).
2. **Full `AI_CLOSED`** requires PD-15 Coq discharge (future task).
3. **Partial materialization** is valid for TASK-PROOF-013; full 12/12 batch verification remains
   [PROOF-008](./PROOF-008-RUNTIME-VECTOR-VERIFICATION.md) scope after `golden_vector_materializer.sh`.
4. No source files under `core/src/consensus*`, `state_hash`, or `formal_verification/coq` were
   modified for this task.

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-013` — runtime witness observation for A-1 partial closure |
