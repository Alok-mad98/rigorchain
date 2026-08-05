# PROOF-034 Height Tracker

**Task:** `TASK-PROOF-034-CAMPAIGN-001`  
**Updated:** 2026-06-24  
**equivalence_claimed:** `false`

---

## Current Progress

| Field | Value | Source |
|-------|-------|--------|
| **Current chain height** | **UNAVAILABLE** | RPC `getblockcount` failed (2026-06-24): `Could not connect to 127.0.0.1:28761` |
| **Target height** | **2,100,001** | `proof034_observer.py` `TARGET_HEIGHT_DEFAULT`; `PROOF034_TARGET_HEIGHT` env |
| **Remaining blocks** | **UNAVAILABLE** | Requires live height |
| **Progress percentage** | **UNAVAILABLE** | Observer: `progress_percentage: null` |
| **Estimated completion date** | **UNAVAILABLE** | Requires live block rate; observer `eta_human: null` |

**Last observer snapshot:** `2026-06-17T14:06:47Z` — `.runtime/proof034/proof034_observation.json`

**Last known height:** None recorded (RPC was already unavailable at last snapshot).

---

## Milestone Progression

| Order | Milestone | Height | Status (live) | Status (SIM fixture) |
|-------|-----------|--------|---------------|----------------------|
| — | Low-height vectors (9/12) | ≤ 501 | Materialized per PROOF-034 §2 | N/A |
| 1 | **GV-ORU-0003** | 630,000 | **PENDING** | ESTABLISHED / GVR_PASS |
| 2 | **GV-RES-0003** | 2,100,000 | **PENDING** | ESTABLISHED / GVR_PASS |
| 3 | **GV-DRAW-0003** (final) | 2,100,001 | **PENDING** | ESTABLISHED / GVR_PARTIAL |

| Field | Value |
|-------|-------|
| **Current milestone** | None reached on live chain (height unknown) |
| **Next milestone** | GV-ORU-0003 @ 630,000 (first height-gated vector) |
| **Final milestone** | GV-DRAW-0003 @ 2,100,001 |

---

## RPC Status

| Check | Result (2026-06-24) |
|-------|---------------------|
| `rigor-cli` binary | Present: `core/src/rigor-cli` |
| `rigord` running | **No** (connection timeout) |
| Datadir | `~/.rigor/golden-vector-archive-regtest` exists |
| Live fixtures | **Absent** (`regtest/golden_vector_fixtures.json` not found) |
| Live harness report | **Absent** (`golden_vector_harness_report.json` not found) |

---

## Observer Artifacts

| File | Last update | Height recorded |
|------|-------------|-----------------|
| `.runtime/proof034/proof034_observation.json` | 2026-06-17T14:06:47Z | `null` |
| `.runtime/proof034/proof034_realtime_snapshot.json` | 2026-06-17T14:06:47Z | `null` |
| `.runtime/proof034/proof034_final_report_template.json` | 2026-06-17T14:06:47Z | — |

---

## Offline Fixture Reference (not live height)

`sim/proof_validator/fixtures/archive/golden_vector_fixtures.json` declares:

| Field | Value |
|-------|-------|
| `archive_height` | 2,100,001 |
| `chain_height` | 2,100,001 |

This is a **repository fixture for SIM-006** only. It does **not** reflect live regtest chain height.

---

## How to Refresh This Tracker

```bash
# 1. Ensure node is running
./core/src/rigord -regtest -daemon -datadir="${HOME}/.rigor/golden-vector-archive-regtest"
./core/src/rigor-cli -regtest -datadir="${HOME}/.rigor/golden-vector-archive-regtest" getblockcount

# 2. Run observer once
python3 runtime/proof034/observer/proof034_observer.py --once

# 3. Read updated snapshot
cat .runtime/proof034/proof034_observation.json | jq '.proof_progress'
```

---

## Campaign End Condition

```
chain_height >= 2_100_001
AND proof_progress.complete == true
AND all milestone vectors verified on live harness
```

Until RPC returns a height, **no progress percentage or ETA may be reported**.

---

*Height tracker — values invented only from evidence; unavailable fields explicitly marked.*
