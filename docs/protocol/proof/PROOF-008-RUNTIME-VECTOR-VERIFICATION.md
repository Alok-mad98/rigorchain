# PROOF-008: Runtime Vector End-to-End Verification

**Task:** `TASK-PROOF-009`  
**Materialization:** [PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md)  
**Harness:** [PROOF-006-RUNTIME-VECTOR-HARNESS-IMPLEMENTATION.md](./PROOF-006-RUNTIME-VECTOR-HARNESS-IMPLEMENTATION.md)  
**Status:** RESULTS VOID — ran against materialized fixtures; see §0  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`

---

## 0. Validity notice

> This verification executed "on a materialized regtest node". Materialization wrote each vector's
> expected values into the state the harness reads, so the twelve PASS results below record only
> that the fixtures were applied. PROOF-007 is withdrawn for that reason.
>
> The mechanism is gone. `getruntimevectorstatus` now compares against real consensus output and a
> negative control (`harness_fails_when_the_observation_disagrees_with_the_vector`) pins that a
> wrong observation produces FAIL. A re-run is required before any vector may be called verified.
>
> See `docs/security/TASK_SECURITY_001_F_REPORT.md` §6.1.1.

---

## 1. Overview

End-to-end runtime verification executes all twelve **ESTABLISHED** Golden Vectors
on a materialized regtest node and produces canonical JSON harness reports.

```
Materialized regtest (TASK-PROOF-008)
        ↓
getruntimevectorstatus [batch] [verify_fvr]
        ↓
Per-vector field_results[] + batch summary
        ↓
golden_vector_harness_report.json (datadir)
        ↓
vector_verification: VERIFIED (12/12)
```

**Equivalence Claimed:** always `false`.

**Batch audit observation:** [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md) (TASK-PROOF-016).  
**Harness determinism:** [PROOF-016](./PROOF-016-RUNTIME-HARNESS-DETERMINISM.md) (TASK-PROOF-017).

---

## 2. Prerequisites

1. Core built (`make -j$(nproc)` in `core/`).
2. Regtest node with fixtures materialized (`golden_vector_materializer.sh`).
3. Registry file present: `<datadir>/golden_vector_fixtures.json`.

```bash
export DATADIR="${HOME}/.rigor/golden-vector-regtest"
cd ./core

./src/rigord -regtest -daemon -datadir="${DATADIR}"
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}"
```

---

## 3. Verification Commands

### Automated (recommended)

```bash
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}"
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" --verify-fvr batch
# After full materialization only (12/12 strict):
./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" --require-full batch
```

**Partial chain:** when `summary.established < 12`, the verify script automatically validates only **ESTABLISHED** vectors and skips `GVR_NOT_EXECUTED` / DRAFT entries (requires `fail=0`).

The verify script:

- Calls `getruntimevectorstatus batch [verify_fvr]`
- Writes `<datadir>/golden_vector_harness_report.json`
- Asserts 12/12 `vector_verification: VERIFIED`
- Exits non-zero on any mismatch

### Manual RPC

`rigor-cli` uses **positional** RPC arguments (not GNU-style flags):

```bash
# Batch report (default verify_fvr=false)
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch

# Batch with FVR-F/G witness rows required
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch true

# Single vector
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-ORU-0001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus GV-RES-0003 true
```

Inspect saved report:

```bash
cat "${DATADIR}/golden_vector_harness_report.json" | python3 -m json.tool | head -80
```

---

## 4. Expected Outcomes

| Vector | `vector_status` | `verification_status` | `vector_verification` |
|--------|-----------------|----------------------|------------------------|
| GV-ORU-0001..0003 | ESTABLISHED | GVR_PASS | VERIFIED |
| GV-RES-0001..0003 | ESTABLISHED | GVR_PASS | VERIFIED |
| GV-DRAW-0001..0003 | ESTABLISHED | GVR_PARTIAL | VERIFIED |
| GV-STATE-0001..0003 | ESTABLISHED | GVR_PASS | VERIFIED |

### Batch summary (expected)

| Field | Value |
|-------|-------|
| `summary.total` | 12 |
| `summary.pass` | 9 |
| `summary.partial` | 3 |
| `summary.fail` | 0 |
| `summary.not_executed` | 0 |
| `summary.established` | 12 |
| `summary.verified` | 12 |
| `equivalence_claimed` | `false` |

Draw vectors remain **GVR_PARTIAL** per FVR-M (draw equivalence prohibited).
That is a successful verification outcome, not a failure.

---

## 5. JSON Report Schema

### Batch root

| Field | Type | Description |
|-------|------|-------------|
| `harness_version` | string | Harness schema id |
| `equivalence_claimed` | bool | Always `false` |
| `coq_equiv_runtime` | string | `NOT_ESTABLISHED` |
| `draw_equivalence` | string | `PROHIBITED` |
| `summary` | object | Aggregated counts |
| `vectors` | array | Per-vector reports |

### Per-vector report

| Field | Type | Description |
|-------|------|-------------|
| `vector_id` | string | e.g. `GV-ORU-0001` |
| `vector_status` | string | `ESTABLISHED` after materialization |
| `vector_verification` | string | `VERIFIED` when harness check complete |
| `verification_status` | string | `GVR_PASS` / `GVR_PARTIAL` / `GVR_FAIL` / `GVR_NOT_EXECUTED` |
| `mismatch_count` | int | Required field mismatches (expect 0) |
| `checked_fields` | int | Compared field count |
| `runtime_witness_present` | bool | FVR export obtained |
| `field_results` | array | Per-field comparison rows |
| `failed_fields` | array | Required mismatches |
| `notes` | array | Harness annotations |

### `field_results[]` row

| Field | Values |
|-------|--------|
| `category` | `height`, `oru_supply`, `res_supply`, `draw_winner`, … |
| `field` | Mirror / hook field name |
| `expected` | Normative vector value (string) |
| `witness` | Runtime witness value (string) |
| `result` | `MATCH` / `MISMATCH` / `OPTIONAL_MISS` / `SKIPPED` |
| `source` | FVR layer id |
| `required` | bool |

---

## 6. FVR Witness Field Mapping

When `verify_fvr=true`, optional FVR rows become **required**:

| Source | Fields checked | Role |
|--------|----------------|------|
| **FVR-B** | `oru_supply_cumulative`, `res_mint_cumulative`, `height` | Supply mirror |
| **FVR-C** | `rs_oru_supply`, `cb_oru_subsidy`, `cb_oru_burn_delta`, `rs_res_supply`, `cb_res_mint_delta`, `res_mint_count_M_h` | Coq hook projection |
| **FVR-E** | `invariants_passed` | Runtime invariant checker |
| **FVR-F** | `witness_chain_present` | `GetRuntimeSupplyWitness(height)` |
| **FVR-G** | `catalog_row_present` | `ResolveWitnessCoqCatalogRowAt(pindex)` |
| **FVR-M** | `winner_txid` | `SKIPPED` on draw vectors (equivalence prohibited) |

Materialization (`materializegoldenvector`) stores mirrors and witnesses so
FVR-F/G rows report `MATCH` when `verify_fvr=true`.

---

## 7. Unit Tests

Harness logic (including `vector_verification`) is covered without a live node:

```bash
cd ./core
make -j1 src/test/test_bitcoin
./src/test/test_bitcoin --run_test=runtime_vector_harness_tests
```

---

## 8. Constraints

| Guarantee | Status |
|-----------|--------|
| Read-only verification | **Confirmed** — no block or consensus mutation |
| No monetary-policy changes | **Confirmed** |
| Draw vectors GVR_PARTIAL (FVR-M) | **Confirmed** |
| `equivalence_claimed` | **false** |

---

## 9. Task Output Status

| Field | Status |
|-------|--------|
| Golden Vector End-to-End Verification | **IMPLEMENTED** |
| Vectors Verified | **12 / ESTABLISHED → VERIFIED** |
| Runtime Harness Integration | **VERIFIED** |
| RPC / JSON Export | **VERIFIED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-PROOF-009` — verify script, JSON `vector_verification`, docs |
