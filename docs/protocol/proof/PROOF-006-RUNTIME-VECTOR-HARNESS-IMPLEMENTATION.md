# PROOF-006: Runtime Vector Harness Implementation

**Task:** `TASK-PROOF-007`  
**Specification:** [PROOF-005-RUNTIME-VECTOR-HARNESS.md](./PROOF-005-RUNTIME-VECTOR-HARNESS.md)  
**Status:** IMPLEMENTED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`

---

## 1. Overview

Executable C++ implementation of the Golden Runtime Verification Harness:

```
Golden Vector Registry (12 entries)
        ↓
RuntimeWitnessSnapshot (FVR-B/C/E/F/G read-only)
        ↓
Field comparison
        ↓
GVR_* verification report (JSON via RPC)
```

**Equivalence Claimed:** always `false`.

---

## 2. Source Files

| File | Role |
|------|------|
| `core/src/proof/runtime_vector_harness.h` | Types, registry API, evaluation |
| `core/src/proof/runtime_vector_harness.cpp` | 12-vector registry + comparison engine |
| `core/src/test/runtime_vector_harness_tests.cpp` | Unit tests |
| `core/src/rpc/blockchain.cpp` | `getruntimevectorstatus` RPC |

Build integration:

- `core/src/Makefile.am` — `proof/runtime_vector_harness.cpp` in `libbitcoin_node`
- `core/src/Makefile.test.include` — harness unit tests

---

## 3. Vector Registry

Twelve normative entries embedded from PROOF-001..004:

| ID | Height | Family |
|----|--------|--------|
| GV-ORU-0001 | 0 | ORU |
| GV-ORU-0002 | 1 | ORU |
| GV-ORU-0003 | 630000 | ORU |
| GV-RES-0001 | 10 | RES |
| GV-RES-0002 | 500 | RES |
| GV-RES-0003 | 2100000 | RES |
| GV-DRAW-0001 | 1 | DRAW |
| GV-DRAW-0002 | 7 | DRAW |
| GV-DRAW-0003 | 2100001 | DRAW |
| GV-STATE-0001 | 0 | STATE |
| GV-STATE-0002 | 10 | STATE |
| GV-STATE-0003 | 6 | STATE |

Each entry stores expected atom values and `fixture_materialized = false` until
functional fixture chains are wired.

---

## 4. Verification Status

| Code | Meaning |
|------|---------|
| `GVR_NOT_EXECUTED` | Fixture not materialized (default for all 12) |
| `GVR_PASS` | All required fields match |
| `GVR_FAIL` | Required field mismatch |
| `GVR_PARTIAL` | Optional gap or draw fields (FVR-M) |

---

## 5. FVR Integration (Read-Only)

`CaptureRuntimeWitnessSnapshot` observes:

| Layer | Source |
|-------|--------|
| FVR-B/D | `BuildRuntimeSupplyExport` |
| FVR-C | `CoqSupplyFieldHook` in export |
| FVR-E | `VerifyRuntimeSupplyInvariants` |
| FVR-F | `GetRuntimeSupplyWitness` |
| FVR-G | `ResolveWitnessCoqCatalogRowAt` |

No FVR source files were modified.

---

## 6. RPC

### `getruntimevectorstatus [target]`

| `target` | Response |
|----------|----------|
| *(omit)* or `"batch"` | Batch report with `summary` + `vectors[]` |
| `"GV-ORU-0001"` etc. | Single-vector report |

Examples:

```bash
cd ./core
./src/rigor-cli getruntimevectorstatus
./src/rigor-cli getruntimevectorstatus batch
./src/rigor-cli getruntimevectorstatus GV-RES-0001
```

Response always includes:

- `equivalence_claimed: false`
- `coq_equiv_runtime: "NOT_ESTABLISHED"`
- `draw_equivalence: "PROHIBITED"`

---

## 7. Build and Test

```bash
cd ./core
make -j"$(nproc)"
make -j"$(nproc)" src/test/test_bitcoin
./src/test/test_bitcoin --run_test=runtime_vector_harness_tests
```

Expected unit-test behavior:

- All 12 vectors → `GVR_NOT_EXECUTED` with default fixtures
- Synthetic mirror test → `GVR_PASS` for GV-ORU-0001
- Draw vector → `GVR_PARTIAL` when materialized (FVR-M)

---

## 8. Wiring Fixture Blocks (Future)

To promote vectors from `NOT_EXECUTED` to live comparison:

1. Implement regtest/functional fixture chain matching PROOF-001..004 heights and supplies.
2. Set `fixture_materialized = true` in registry for wired vectors (or call
   `SetGoldenVectorFixtureMaterializedForTest` in functional harness only).
3. Re-run `getruntimevectorstatus batch` — expect `GVR_PASS` / `GVR_PARTIAL` on supply fields.
4. Promote vector docs `DRAFT → ESTABLISHED` per PROOF-005 promotion criteria.

Draw vectors remain **`GVR_PARTIAL` maximum** (FVR-M).

---

## 9. Future Coq Integration

Not implemented in TASK-PROOF-007. Planned chain:

```
Vector → Runtime Harness → Coq Compute → Candidate Equivalence
```

See PROOF-005 §8.

---

## 10. Freeze Compatibility

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No validation modifications | **Confirmed** |
| No state_hash modifications | **Confirmed** |
| No FVR modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |

---

## Task Output Status

| Field | Status |
|-------|--------|
| Runtime Vector Harness | **IMPLEMENTED** |
| Vector Comparison Categories | **IMPLEMENTED** |
| Verification Result Types | **IMPLEMENTED** |
| Golden Vector Targets | **12 / DRAFT → EXECUTED** (harness live; fixtures pending) |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-007` — C++ harness, tests, RPC, implementation doc |
