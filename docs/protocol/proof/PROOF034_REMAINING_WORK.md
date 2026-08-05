# PROOF-034 Remaining Work

**Task:** `TASK-PROOF-034-CAMPAIGN-001`  
**Updated:** 2026-06-24  
**equivalence_claimed:** `false`

---

## What Still Prevents PROOF-034 Completion?

PROOF-034 closes only when a **live regtest chain** reaches height **≥ 2,100,001**, all **12 golden vectors** are **ESTABLISHED**, the C++ harness reports **9 PASS + 3 PARTIAL + 0 FAIL + 0 NOT_EXECUTED**, and the archive is exported with a **SHA-256 manifest**.

**Today:** `proof034_complete = false`. Live RPC is unavailable. No `golden_vector_fixtures.json` or `golden_vector_harness_report.json` exists in the campaign datadir. Observer shows all three milestones **pending**.

---

## Completed Items

| Item | Evidence |
|------|----------|
| PROOF-034 procedure document | `PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` — ESTABLISHED |
| C++ runtime vector harness (12 vectors) | `core/src/proof/runtime_vector_harness.cpp` — IMPLEMENTED |
| Materialization tooling | `core/contrib/tools/golden_vector_materializer.sh` (`MAX_VECTOR_HEIGHT=2100001`) |
| Verification tooling | `core/contrib/tools/golden_vector_verify.sh` |
| Archive monitor | `core/contrib/tools/golden_vector_archive_monitor.sh` |
| Passive observer | `runtime/proof034/observer/proof034_observer.py` |
| Milestone watch | `runtime/proof034/observer/golden_vector_watch.py` |
| SIM-006 catalog + checks | `sim/proof_validator/catalog.py`, `checks.py` |
| Offline fixture (3 milestones) | `sim/proof_validator/fixtures/archive/golden_vector_fixtures.json` |
| Coq Phase-5 closure | PROOF-018–033 ESTABLISHED |
| Low-height vectors (9/12) | PROOF-034 §2: materialized @ h≤501 |
| Pool/ops JSONL telemetry | Observer: 1687+ miner shares, 2173 job lifecycle lines present |
| Final report template | `docs/proof/PROOF034_FINAL_REPORT_TEMPLATE.md` |

---

## Partially Completed Items

| Item | Done | Remaining |
|------|------|-----------|
| Golden vector catalog | 12 defined in C++; 9 materialized at low height | 3 height-gated vectors need live chain |
| Observer deployment | Scripts + snapshot files exist | RPC unavailable; snapshot stale (2026-06-17) |
| Archive integrity | JSONL metrics validated | SHA-256 manifest absent |
| SIM-006 validation | Offline fixture passes catalog checks | Does not prove live regtest state |
| Public testnet campaign | Plan ACTIVE (`RIGOR_VALIDATION_CAMPAIGN_PLAN.md`) | All checkpoints PENDING |
| PROOF-001..004 vector docs | Framework ESTABLISHED | Individual entries still DRAFT |

---

## Pending Items

| # | Item | Owner action |
|---|------|--------------|
| 1 | Start regtest `rigord` on campaign datadir | Operator |
| 2 | Run `golden_vector_materializer.sh` to mine h≥2,100,001 | Operator |
| 3 | Materialize **GV-ORU-0003** @ 630,000 on live chain | Automatic via materializer |
| 4 | Materialize **GV-RES-0003** @ 2,100,000 on live chain | Automatic via materializer |
| 5 | Materialize **GV-DRAW-0003** @ 2,100,001 on live chain | Automatic via materializer |
| 6 | Run `golden_vector_verify.sh --require-full batch` | Operator |
| 7 | Export archive to `$DATADIR/archive/proof-034/` with manifest | Operator (PROOF-034 §4.6) |
| 8 | Refresh observer (`proof034_observer.py --once`) | Operator |
| 9 | Complete `PROOF034_FINAL_REPORT_TEMPLATE.md` with live witnesses | Operator |
| 10 | Set `proof034_complete` true in release metrics (gate clears G6-PROOF-01) | Automatic when height + harness pass |

---

## Blocking Items

| Blocker | Severity | Resolution |
|---------|----------|------------|
| **No live regtest node** | CRITICAL | Start `rigord -regtest -datadir=~/.rigor/golden-vector-archive-regtest` |
| **Chain not mined to 2,100,001** | CRITICAL | Run materializer (batch mining) |
| **No live fixture/harness JSON in datadir** | CRITICAL | Materializer + verify scripts |
| **No archive SHA-256 manifest** | HIGH | Complete §4.6 export procedure |
| **Observer RPC unavailable** | MEDIUM | Restore node; re-run observer |
| **SIM fixture ≠ live evidence** | INFORMATIONAL | Do not treat offline fixture as campaign closure |

**Non-blockers (explicitly out of scope for PROOF-034):**
- CPP-01 closure (permanently false per PROOF-033)
- Global equivalence (`equivalence_claimed` must remain false)
- Coq re-proof (already ESTABLISHED)

---

## Operational Dependencies

```
core built (rigord, rigor-cli)
    ↓
regtest node running (:28761)
    ↓
golden_vector_materializer.sh (mines + materializes)
    ↓
golden_vector_verify.sh (harness evaluation)
    ↓
archive export + SHA-256 manifest
    ↓
proof034_observer.py (witness snapshot)
    ↓
proof034_complete → release gate G6
```

**Environment defaults:**
- `DATADIR`: `~/.rigor/golden-vector-archive-regtest`
- `ARCHIVE_DIR`: `$DATADIR/archive/proof-034`
- `PROOF034_TARGET_HEIGHT`: `2100001`

---

## Human / Operator Actions Required

1. **Build core** (user-local): `cd core && make -j$(nproc)`
2. **Start regtest node** with campaign datadir and wallet `golden_vector_archive`
3. **Run materializer** to target height 2,100,001 (long-running)
4. **Run verifier** with `--require-full` after materialization completes
5. **Export archive** per PROOF-034 §4.6
6. **Run observer** to capture final snapshot
7. **Fill final report** from `proof034_final_report_template.json`
8. **(Parallel)** Public testnet campaign to same milestone heights — separate track, same vector definitions

---

## Completion Checklist

- [ ] `getblockcount` ≥ 2,100,001 on campaign regtest
- [ ] `golden_vector_fixtures.json` in datadir with 12 vectors ESTABLISHED
- [ ] `golden_vector_harness_report.json`: 9 PASS, 3 PARTIAL, 0 FAIL, 0 NOT_EXECUTED
- [ ] Archive manifest SHA-256 verified
- [ ] Observer `proof_progress.complete = true`
- [ ] `proof034_complete = true` in gate engine
- [ ] Final report signed per template

---

*End of remaining work analysis.*
