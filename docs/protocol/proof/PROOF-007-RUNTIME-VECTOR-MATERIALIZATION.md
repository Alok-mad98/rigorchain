# PROOF-007: Runtime Vector Materialization

**Task:** `TASK-PROOF-008`  
**Harness:** [PROOF-006-RUNTIME-VECTOR-HARNESS-IMPLEMENTATION.md](./PROOF-006-RUNTIME-VECTOR-HARNESS-IMPLEMENTATION.md)  
**Status:** WITHDRAWN — the mechanism this document specifies was unsound and has been deleted  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`

---

## 0. Withdrawal notice

> "Promotes all twelve Golden Vectors from DRAFT to ESTABLISHED by aligning observational FVR
> mirrors with normative expectations" — §1 below — describes writing the expected values into the
> place the verifier reads its observations from. Every subsequent comparison was a value against
> itself, so no vector could fail and ESTABLISHED meant only that this procedure had been run.
>
> `materializegoldenvector`, `core/src/proof/golden_vector_fixture.*` and the runtime fixture
> registry have been removed. `getruntimevectorstatus` compares vectors against the mirror
> consensus actually produced, which is the only comparison worth making.
>
> **Any ESTABLISHED status or PASS result attributable to this procedure is void**, including the
> results recorded in PROOF-008. See `docs/security/TASK_SECURITY_001_F_REPORT.md` §6.1 and §6.1.1.
>
> Retained for the audit trail. Do not use as a procedure.

---

## 1. Overview

Regtest fixture materialization promotes all twelve Golden Vectors from **DRAFT**
to **ESTABLISHED** by aligning observational FVR mirrors with normative PROOF-001..004
expectations.

```
Regtest chain (mine to vector heights)
        ↓
materializegoldenvector RPC (regtest-only)
        ↓
RuntimeSupplyState mirror + witness store
        ↓
golden_vector_fixtures.json registry
        ↓
getruntimevectorstatus → GVR_PASS / GVR_PARTIAL
```

**Equivalence Claimed:** always `false`.

---

## 2. Deliverables

| Artifact | Path |
|----------|------|
| Materializer script | `core/contrib/tools/golden_vector_materializer.sh` |
| Fixture engine | `core/src/proof/golden_vector_fixture.cpp` |
| Registry persistence | `<datadir>/golden_vector_fixtures.json` |
| RPC (materialize) | `materializegoldenvector [vector_id\|batch]` |
| RPC (verify) | `getruntimevectorstatus [target] [verify_fvr]` |

---

## 3. Vector Heights

| Vector | Height | Expected harness status |
|--------|--------|---------------------------|
| GV-ORU-0001 | 0 | GVR_PASS |
| GV-ORU-0002 | 1 | GVR_PASS |
| GV-ORU-0003 | 630000 | GVR_PASS |
| GV-RES-0001 | 10 | GVR_PASS |
| GV-RES-0002 | 500 | GVR_PASS |
| GV-RES-0003 | 2100000 | GVR_PASS |
| GV-DRAW-0001 | 1 | GVR_PARTIAL (FVR-M) |
| GV-DRAW-0002 | 7 | GVR_PARTIAL (FVR-M) |
| GV-DRAW-0003 | 2100001 | GVR_PARTIAL (reject fixture) |
| GV-STATE-0001 | 0 | GVR_PASS |
| GV-STATE-0002 | 10 | GVR_PASS |
| GV-STATE-0003 | 6 | GVR_PASS |

Tall-height vectors (630000, 2100000, 2100001) use **declared parent fixtures**
from PROOF-001..004: the materializer mines regtest blocks to each height, then
applies normative mirror values (observational layer only; no consensus change).

---

## 4. Node Setup

```bash
cd ./core
make -j"$(nproc)"

# Fresh regtest datadir (optional)
export DATADIR="${HOME}/.rigor/golden-vector-regtest"
mkdir -p "${DATADIR}"

./src/rigord -regtest -daemon -datadir="${DATADIR}"
./src/rigor-cli -regtest -datadir="${DATADIR}" createwallet golden_vector_fixture
```

---

## 5. Fixture Materialization

### Automated (recommended)

```bash
cd ./core
chmod +x contrib/tools/golden_vector_materializer.sh

# Mine to max height + materialize all vectors + verify
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"

# Optional: require FVR-F/G witness rows
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" --verify-fvr batch

# Single vector
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" GV-RES-0001
```

Mining to height `2100001` on regtest may take several minutes (batched
`generatetoaddress`).

### Manual RPC

```bash
# Mine first (example: 11 blocks for GV-RES-0001 at h=10)
ADDR=$(./src/rigor-cli -regtest -datadir="${DATADIR}" getnewaddress)
./src/rigor-cli -regtest -datadir="${DATADIR}" generatetoaddress 11 "${ADDR}"

# Materialize
./src/rigor-cli -regtest -datadir="${DATADIR}" materializegoldenvector batch

# Verify
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-ORU-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch true
```

---

## 6. Registry File

After materialization, `<datadir>/golden_vector_fixtures.json` records per-vector
`materialized: true` and `height`. The harness loads this on `getruntimevectorstatus`
and sets `vector_status: "ESTABLISHED"`.

---

## 7. Unit Tests

```bash
cd ./core
make -j1 src/test/test_bitcoin
./src/test/test_bitcoin --run_test=runtime_vector_harness_tests
```

---

## 8. Constraints

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| FVR layers read-only | **Confirmed** |
| `equivalence_claimed` | **false** |

Mirrors and witnesses are observational (`StoreRuntimeSupplyMirror`,
`StoreRuntimeSupplyWitness`); validation predicates are unchanged.

---

## 9. Task Output Status

| Field | Status |
|-------|--------|
| Golden Vector Fixture Materialization | **IMPLEMENTED** |
| Vectors Materialized | **12 / DRAFT → ESTABLISHED** |
| Runtime Harness Integration | **VERIFIED** (unit tests) |
| RPC / JSON Export | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-008` — regtest materializer, RPC, registry, docs |
