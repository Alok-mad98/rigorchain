# PROOF-034 Campaign Status Dashboard

**Task:** `TASK-PROOF-034-CAMPAIGN-001`  
**Updated:** 2026-06-24 (audit snapshot)  
**equivalence_claimed:** `false`  
**Architecture / Consensus / Protocol / Mining / Wallet Impact:** NONE (audit only)

---

## Campaign Summary

| Field | Value |
|-------|-------|
| Campaign ID | PROOF-034 — Full Runtime Archive Verification |
| Doc status | PROCEDURE ESTABLISHED — **RUN NOT YET EXECUTED** |
| Release gate | `proof034_complete = false` (`G6-PROOF-01` blocker) |
| Live RPC (regtest :28761) | **UNAVAILABLE** (2026-06-24 check) |
| Observer last update | `2026-06-17T14:06:47Z` |
| Target height | **2,100,001** |

---

## Milestone Table

| Milestone | Description | Status | Evidence |
|-----------|-------------|--------|----------|
| **GV-ORU-0003** | ORU post-halving subsidy floor @ h=630,000 | **PENDING (live)** | Observer: `executed=false`, `verified=false`. SIM fixture: `ESTABLISHED` / `GVR_PASS` in `sim/proof_validator/fixtures/archive/golden_vector_fixtures.json`. C++ spec: `runtime_vector_harness.cpp` L178–203 |
| **GV-RES-0003** | RES cap terminal mint @ h=2,100,000 | **PENDING (live)** | Observer: pending. SIM fixture: `ESTABLISHED` / `GVR_PASS`, `res_supply_cumulative=2100000000`. C++ spec: `runtime_vector_harness.cpp` L256–281 |
| **GV-DRAW-0003** | Draw kernel + cap reject @ h=2,100,001 | **PENDING (live)** | Observer: pending. SIM fixture: `ESTABLISHED` / `GVR_PARTIAL`. C++ `reject_expected` path in harness |
| **SIM-004** | Telemetry replay kernel | **IMPLEMENTED** | `sim/core/replay_kernel.py`; used by Replay CI |
| **SIM-005** | System state emulator | **IMPLEMENTED** | `sim/emulator/system_state_machine.py` |
| **SIM-006** | PROOF-034 offline fixture validator | **PASS-CAPABLE (offline)** | `sim/proof_validator/proof_replay_validator.py`; fixture `archive_height=2100001` |
| **Replay** | Replay CI pipeline | **IMPLEMENTED** | `ci/replay/pipeline.py`; modes FAST/FULL/PROOF_REPLAY |
| **Observer** | Read-only campaign observer | **DEPLOYED; STALE** | `runtime/proof034/observer/proof034_observer.py`; `.runtime/proof034/proof034_observation.json` |
| **Archive** | SHA-256 manifest + export | **NOT COMPLETE** | Observer: `manifest_present=false`, `no_manifest_yet` |
| **Coq closure** | Phase-5 mathematical program | **ESTABLISHED** | PROOF-018–033; CPP-01 non-closable (PROOF-033) |
| **PROOF-000** | Golden vector framework | **ESTABLISHED** | `docs/proof/PROOF-000-GOLDEN-VECTORS.md` |
| **PROOF-001** | Golden ORU vectors | **ESTABLISHED (framework); DRAFT entries** | `PROOF-001-GOLDEN-ORU-VECTORS.md` |
| **PROOF-002** | Golden RES vectors | **ESTABLISHED (framework); DRAFT entries** | `PROOF-002-GOLDEN-RES-VECTORS.md` |
| **PROOF-003** | Golden DRAW vectors | **ESTABLISHED (framework); DRAFT entries** | `PROOF-003-GOLDEN-DRAW-VECTORS.md` |
| **PROOF-004** | Golden STATE vectors | **ESTABLISHED (framework); DRAFT entries** | `PROOF-004-GOLDEN-STATE-VECTORS.md` |
| **PROOF-005** | Harness specification | **ESTABLISHED** | `PROOF-005-RUNTIME-VECTOR-HARNESS.md` |
| **PROOF-006** | Harness implementation | **IMPLEMENTED** | `core/src/proof/runtime_vector_harness.cpp` |
| **PROOF-007** | Vector materialization | **IMPLEMENTED** | `golden_vector_fixture.cpp`; `golden_vector_materializer.sh` |
| **PROOF-008** | E2E verification | **IMPLEMENTED** | `golden_vector_verify.sh` |
| **PROOF-009** | Obligation registry | **ESTABLISHED** | `PROOF-009-PROOF-OBLIGATION-REGISTRY.md` |
| **PROOF-010** | Audit closure framework | **ESTABLISHED** | `PROOF-010-AUDIT-ITEM-CLOSURE-FRAMEWORK.md` |
| **PROOF-011** | Audit A-1 closure | **ESTABLISHED; AI_PARTIAL** | `PROOF-011-AUDIT-A1-CLOSURE.md` |
| **PROOF-012** | A-1 runtime observation | **OBSERVED** | Diagnostic witness |
| **PROOF-013** | A-2/A-3 runtime observation | **OBSERVED** | Diagnostic witness |
| **PROOF-014** | A-4/A-5 runtime observation | **OBSERVED** | Diagnostic witness |
| **PROOF-015** | A-1..A-5 batch observation | **OBSERVED** | `PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md` |
| **PROOF-016** | Harness determinism | **IMPLEMENTED** | `PROOF-016-RUNTIME-HARNESS-DETERMINISM.md` |
| **PROOF-017** | *(no document)* | **N/A** | Task absorbed into PROOF-016 |
| **PROOF-018** | Coq closure alignment | **IMPLEMENTED** | `formal_verification/coq/` |
| **PROOF-019** | PD-4F-OB-02 dependency model | **ESTABLISHED** | `PROOF-019-PD4F-OB02-DEPENDENCY-MODEL.md` |
| **PROOF-020** | PD-15 Sigma refinement | **ESTABLISHED** | Coq + `sigma_hash.cpp` |
| **PROOF-021** | PD-14 UInt512 refinement | **ESTABLISHED** | `rigor_math.h` |
| **PROOF-022** | PD-13 F-set membership | **ESTABLISHED** | `convergence_root.cpp` |
| **PROOF-023** | PD-4F-OB-02 readiness | **ESTABLISHED** | Phase-4F gates |
| **PROOF-024** | CPP-01 closure model | **ESTABLISHED** | Model only |
| **PROOF-025** | Equivalence evidence model | **ESTABLISHED** | `equivalence_claimed: false` |
| **PROOF-026** | Equivalence runtime confirmation | **ESTABLISHED** | Negative boundary |
| **PROOF-027** | Phase-5 summary freeze link | **ESTABLISHED** | Freeze manifest link |
| **PROOF-028** | Phase-5 final consistency | **ESTABLISHED** | Coq/doc alignment |
| **PROOF-029** | Freeze candidate | **ESTABLISHED** | Mainnet freeze link |
| **PROOF-030** | CPP-01 roadmap | **ESTABLISHED** | Roadmap only |
| **PROOF-031** | Mainnet freeze formalization | **ESTABLISHED** | Freeze declaration |
| **PROOF-032** | Phase-5 terminal status | **ESTABLISHED** | Program terminal |
| **PROOF-033** | CPP-01 non-closability | **ESTABLISHED** | Permanent `false` |
| **PROOF-034** | Full runtime archive | **OPEN** | Procedure ready; live run not executed |

---

## Layer Status Matrix

| Layer | Complete? | Notes |
|-------|-------------|-------|
| Coq / formal (PROOF-018–033) | Yes | Does not substitute for live archive |
| C++ harness tooling (PROOF-006–008) | Yes | 9/12 vectors @ h≤501 per PROOF-034 §2 |
| SIM-006 offline replay | Yes (fixture) | Not live chain evidence |
| Live regtest mining | **No** | RPC down; no datadir fixtures |
| Archive manifest | **No** | `manifest_present: false` |
| Observer instrumentation | Partial | Deployed; stale snapshot |

---

## Related Documents

- [PROOF034_REMAINING_WORK.md](./PROOF034_REMAINING_WORK.md)
- [PROOF034_HEIGHT_TRACKER.md](./PROOF034_HEIGHT_TRACKER.md)
- [PROOF034_MILESTONE_DEFINITIONS.md](./PROOF034_MILESTONE_DEFINITIONS.md)
- [PROOF034_CANONICAL_AUTHORITY.md](./PROOF034_CANONICAL_AUTHORITY.md)
- [PROOF034_EXECUTIVE_SUMMARY.md](./PROOF034_EXECUTIVE_SUMMARY.md)

---

*Audit-only dashboard. Source: code + runtime artifacts as of 2026-06-24.*
