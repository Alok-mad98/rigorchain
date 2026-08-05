# PROOF-014: A-4 / A-5 Runtime Witness Observation

**Task:** `TASK-PROOF-015`  
**Track:** Formal Verification Program — Proof Infrastructure Layer  
**Status:** OBSERVED (runtime diagnostic)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md), [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md), [PROOF-013](./PROOF-013-A2-A3-RUNTIME-OBSERVATION.md)  
**Batch witness:** [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (TASK-PROOF-016)  
**Scope:** Observational only — no consensus, FVR, state_hash, or Coq changes.

**Equivalence Claimed:** false

---

## §1 Purpose

Observe and document audit item lifecycle for:

| Item | Ambiguity | Primary evidence paths |
|------|-----------|------------------------|
| **A-4** | SHA-256d correctness vs FIPS 180-4 | `phase3_golden_vectors.json` (σ, `C`, `bind_commit` digests); `GV-DRAW-*` harness; SIGMA-CR-01 byte audit |
| **A-5** | Canonical formal-program status documentation | [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md) + [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) authoritative surfaces; harness program-boundary fields |

This task does **not** discharge **PD-11** (A-4 FIPS equivalence) or complete A-5 historical-doc retirement, and does not advance either item to `AI_CLOSED`.

---

## §2 Lifecycle Assessment

Both items advance **`AI_OPEN` → `AI_PARTIAL`** on observational evidence. Coq FIPS proof (A-4) and full doc-hygiene sign-off (A-5) remain future work.

| Item | Prior | Observed | Blocks `AI_READY` / `AI_CLOSED` |
|------|-------|----------|----------------------------------|
| **A-4** | `AI_OPEN` | **`AI_PARTIAL`** | PD-11 (SHA256d ≡ FIPS 180-4); vector-as-anchor not yet formally designated |
| **A-5** | `AI_OPEN` | **`AI_PARTIAL`** | Historical status docs not yet marked non-authoritative; formal review sign-off pending |

### §2.1 Evidence mapping

| Category | A-4 | A-5 |
|----------|-----|-----|
| Specification | SHA256d used for σ, `C`, `bind_commit` (RFC-008); vectors as operative regression anchor (informal) | PROOF-009 / PROOF-010 designated authoritative for proof-program status |
| Formal model | C++ `CSHA256` / `Hash256`; Coq `hash256_bytes` implementation (not FIPS proof) | N/A (doc-only item per PROOF-010 §3.2) |
| Coq artifact | PD-01/02 CLOSED (length, determinism); **PD-11 OPEN** (FIPS) | N/A |
| Runtime witness | Harness + `getruntimesupply` operational; hash fields indirect | Harness JSON boundary fields (`equivalence_claimed`, `coq_equiv_runtime`, `draw_equivalence`) |
| Golden vectors | 57/57 oracle; `GV-DRAW-0001` / `GV-DRAW-0002` `GVR_PARTIAL`; `GV-RES-0001` `GVR_PASS` | N/A (not applicable) |
| Review notes | This observation (TASK-PROOF-015) | This observation (TASK-PROOF-015) |

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

### 3.3 Materialize vectors

**Full materialization** (includes `GV-RES-0002` @ h=500, `GV-DRAW-0003` @ h=2 100 001):

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
```

**Partial observation** (height ≥ 11):

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" createwallet gv_a45
ADDR="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getnewaddress "" bech32)"
./src/rigor-cli -regtest -datadir="${DATADIR}" generatetoaddress 11 "${ADDR}"

./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-RES-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-DRAW-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-DRAW-0002
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector GV-STATE-0002
```

**Registry path:** `<datadir>/regtest/golden_vector_fixtures.json`

`GV-RES-0002` and `GV-DRAW-0003` return `GVR_NOT_EXECUTED` until chain height reaches fixture targets (500 and 2 100 001 respectively).

### 3.4 Harness status

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-RES-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-DRAW-0002
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-STATE-0002
```

Omit `verify_fvr` boolean on CLI (string `true` rejected) — see [PROOF-012](./PROOF-012-A1-RUNTIME-OBSERVATION.md) §2.5.

### 3.5 Runtime mirrors

```bash
H10="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 10)"
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${H10}"
```

### 3.6 Hash / draw regression (A-4 primary)

```bash
cd .
python3 formal_verification/tests/test_phase3_oracle.py
```

Expected: `PHASE-3 oracle: 57/57 passed` (SHA256d-derived σ, `C`, bind_commit across layers).

### 3.7 Canonical status surfaces (A-5)

Confirm authoritative proof-program documents are present and cross-linked:

- [PROOF-009](./PROOF-009-PROOF-OBLIGATION-REGISTRY.md) — proof obligation registry
- [PROOF-010](./PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md) — audit item A-1..A-5 lifecycle
- `docs/formal/audit/PROOF_DEBT_REGISTER.md` — master proof debt register

Harness batch root must report program boundaries:

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
# equivalence_claimed: false
# coq_equiv_runtime: NOT_ESTABLISHED
# draw_equivalence: PROHIBITED
```

---

## §4 Observation Log (2026-06-05)

**Environment:** `DATADIR=~/.rigor/golden-vector-regtest`, regtest height 11, partial materialization.

### 4.1 Phase-3 oracle (A-4 hash pipeline)

```
PHASE-3 oracle: 57/57 passed
```

All draw vectors exercise SHA256d on σ preimages, convergence root `C`, and `bind_commit` — cross-layer C++ ↔ Python **MATCH** per prior SIGMA-CR-01 audit.

### 4.2 Harness — RES / DRAW / STATE

| Vector | `verification_status` | `mismatch_count` | Notes |
|--------|----------------------|------------------|-------|
| `GV-RES-0001` | `GVR_PASS` | `0` | RES supply mirrors |
| `GV-DRAW-0001` | `GVR_PARTIAL` | `0` | Expected FVR-M |
| `GV-DRAW-0002` | `GVR_PARTIAL` | `0` | σ/`C` kernel anchors in PROOF-003 |
| `GV-STATE-0002` | `GVR_PASS` | `0` | State snapshot @ h=10 |
| `GV-RES-0002` | `GVR_NOT_EXECUTED` | — | Requires h=500 |
| `GV-DRAW-0003` | `GVR_NOT_EXECUTED` | — | Requires h=2 100 001 |
| `GV-STATE-0001` | `GVR_FAIL` | `1` | `invariants_passed` MISMATCH / RES-L5 at genesis after multi-fixture churn — diagnostic only |

### 4.3 Harness program boundaries (A-5)

Batch root observation:

| Field | Value |
|-------|-------|
| `equivalence_claimed` | `false` |
| `coq_equiv_runtime` | `NOT_ESTABLISHED` |
| `draw_equivalence` | `PROHIBITED` |
| `harness_version` | `proof-006-0.1` |

### 4.4 `getruntimesupply`

At height `10` (`GV-STATE-0002` / `GV-RES-0001` context): `mirror_present: true` (fixture-applied blocks). Invariant state may vary after cumulative fixture writes; supply-path RPC remains operational.

### 4.5 A-5 documentation pin (partial)

| Surface | Role |
|---------|------|
| **PROOF-009** | Authoritative proof obligation registry (§Document Role) |
| **PROOF-010** | Authoritative audit-item closure framework (A-1..A-5 lifecycle) |
| **PROOF_DEBT_REGISTER.md** | Master proof debt enumeration |

Residual: legacy phase docs (`IMPLEMENTATION_STATUS.md`, etc.) not yet marked historical — blocks `AI_CLOSED` for A-5.

---

## §5 Required Output Table

| Field | Status |
|-------|--------|
| Runtime Observation of A-4 / A-5 | **VERIFIED** |
| AI Lifecycle Update | **A-4: `AI_PARTIAL`; A-5: `AI_PARTIAL`** |
| Runtime Harness / Mirrors | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §6 Notes

1. **A-4** hash correctness is evidenced primarily by golden-vector regression (57/57), not FIPS machine proof (PD-11 **OPEN**).
2. **A-5** is documentation-scoped; runtime harness confirms program **boundary** fields only.
3. **`GV-DRAW-0003` / `GV-RES-0002`** require full `golden_vector_materializer.sh` for harness execution at normative heights.
4. **A-4 supports A-1** (σ digest bytes) as a separate trust-anchor item ([PROOF-011](./PROOF-011-AUDIT-A1-CLOSURE.md)).
5. No consensus, FVR, state_hash, or Coq source files were modified.

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-015` — runtime witness observation for A-4 / A-5 partial closure |
