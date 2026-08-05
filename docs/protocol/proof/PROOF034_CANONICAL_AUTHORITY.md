# PROOF-034 Canonical Authority Model

**Task:** `TASK-PROOF-034-CAMPAIGN-001`  
**Updated:** 2026-06-24  
**equivalence_claimed:** `false`

---

## Authority Hierarchy

```
Code (C++ / Python implementation)
    ↓
Runtime evidence (live regtest artifacts)
    ↓
Simulation (offline replay + fixtures)
    ↓
Documentation (PROOF-* / docs/math)
```

**Rule:** When documentation conflicts with code, **code wins**. When simulation fixtures conflict with live runtime artifacts, **live runtime wins**. When live runtime is absent, report **UNAVAILABLE** — do not substitute simulation as closure evidence.

---

## Authoritative Code Files

| Priority | Path | Role |
|----------|------|------|
| 1 | `core/src/consensus/rigor_consensus_lock.h` | Numeric locks: `RES_MAX_SUPPLY`, `RES_REWARD_AMOUNT`, halving |
| 2 | `core/src/proof/runtime_vector_harness.cpp` | 12 golden vector specs + harness evaluation |
| 3 | `core/src/proof/runtime_vector_harness.h` | `GvrVerificationStatus`, `GoldenVectorSpec` struct |
| 4 | `core/src/proof/golden_vector_fixture.cpp` | Live materialization API |
| 5 | `sim/proof_validator/catalog.py` | PROOF-034 milestone catalog (3 vectors) |
| 6 | `sim/proof_validator/checks.py` | SIM-006 validation logic |
| 7 | `formal_verification/spec/rigor_consensus_spec.py` | Draw kernel byte spec |
| 8 | `runtime/proof034/observer/proof034_observer.py` | Observer (read-only) |
| 9 | `runtime/proof034/observer/golden_vector_watch.py` | Milestone watch logic |
| 10 | `core/contrib/tools/golden_vector_materializer.sh` | Mining + materialization |
| 11 | `core/contrib/tools/golden_vector_verify.sh` | Harness verification |
| 12 | `sim/release/gate_engine.py` | `proof034_complete` gate logic |

---

## Authoritative Runtime Artifacts

| Artifact | Path | When authoritative |
|----------|------|-------------------|
| Live fixtures | `$DATADIR/regtest/golden_vector_fixtures.json` | After materializer run on live chain |
| Harness report | `$DATADIR/golden_vector_harness_report.json` | After `golden_vector_verify.sh` |
| Observer state | `.runtime/proof034/proof034_observation.json` | After observer cycle with live RPC |
| Snapshot history | `.runtime/proof034/proof034_realtime_snapshot.json` | Same |
| Archive manifest | `$DATADIR/archive/proof-034/verification_manifest.json` | After §4.6 export |
| Release gate flag | `.runtime/release_gate/release_metrics.json` → `proof034_complete` | When operator sets after closure |

**Current state (2026-06-24):** Live datadir artifacts **absent**. Observer snapshot **stale** (2026-06-17, RPC null).

---

## Authoritative Replay Artifacts

| Artifact | Path | Authority scope |
|----------|------|-----------------|
| SIM-006 fixture | `sim/proof_validator/fixtures/archive/golden_vector_fixtures.json` | Offline catalog alignment only |
| Replay CI hashes | `ci/replay/expected_hashes.json` | Determinism of telemetry replay |
| Sim validator output | `.runtime/sim-proof-validator/` | SIM-006 run artifacts |
| `proof_replay_hash` | Emitted by `proof_replay_validator.py` | Cross-run determinism check |

**Not authoritative for PROOF-034 closure** unless explicitly cross-validated against live harness report.

---

## Authoritative Observer Artifacts

| Component | Reads | Writes |
|-----------|-------|--------|
| `proof034_observer.py` | RPC, fixtures, harness, JSONL, archive | `.runtime/proof034/*.json` |
| `golden_vector_watch.py` | Fixture + harness maps | In-memory status for observer |
| `archive_tracker.py` | SHA manifest, JSONL | Integrity flags |
| `convergence_lag_monitor.py` | `getconvergencestatus`, metrics | Safety flags |

Observer is **read-only** — it does not control mining or materialization.

---

## Informational Documents

| Document | Role | Authority |
|----------|------|-----------|
| `docs/proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` | Procedure | Informational (describes code workflow) |
| `docs/proof/PROOF-000` … `PROOF-033` | Formal program history | Informational |
| `docs/proof/PROOF034_*` (this campaign set) | Operational views | Informational (derived from code + artifacts) |
| `docs/audit/*` | Prior ground-truth audits | Informational |
| `docs/math/*` | Mathematical framing | Informational |
| `docs/freeze/RIGOR_VALIDATION_CAMPAIGN_PLAN.md` | Testnet parallel track | Informational |
| `formal_verification/coq/` | Coq proofs | Informational for runtime closure (Coq ≠ live chain) |
| RFCs, whitepapers | Design history | Informational |

---

## Potentially Stale Documents

| Document | Stale indicator |
|----------|-----------------|
| `PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` §2 | States 9/12 @ h≤501 — may change after materializer re-run |
| `.runtime/proof034/proof034_observation.json` | Last update 2026-06-17; RPC unavailable |
| `docs/math/*` | May reference legacy FORGE/RNG/MTH vocabulary |
| `PROOF-001..004` vector entries | Marked DRAFT despite C++ specs existing |
| `sim/proof_validator/fixtures/archive/golden_vector_fixtures.json` | Claims `archive_height=2100001` without live datadir confirmation |

---

## Conflict Resolution Examples

| Conflict | Resolution |
|----------|------------|
| PROOF-034 doc says RUN NOT EXECUTED; SIM fixture says ESTABLISHED | **Live runtime absent** → doc correct for closure; fixture is offline only |
| `docs/math/` says RNG asset; `rigor_consensus_lock.h` says RES | **Code wins** — RES only |
| Observer height null vs fixture `chain_height=2100001` | **Report unavailable** for live; fixture not substitute |
| PROOF-033 says CPP-01 non-closable; old doc implies equivalence possible | **PROOF-033 wins** (newer established proof) |

---

## Campaign Control Plane Map

```
┌─────────────────────────────────────────────────────────┐
│  AUTHORITATIVE (code)                                    │
│  runtime_vector_harness.cpp  catalog.py  materializer.sh │
└───────────────────────────┬─────────────────────────────┘
                            │ produces (when run)
┌───────────────────────────▼─────────────────────────────┐
│  RUNTIME EVIDENCE (live)                                 │
│  golden_vector_fixtures.json  harness_report.json        │
│  archive/proof-034/manifest                              │
└───────────────────────────┬─────────────────────────────┘
                            │ observed by
┌───────────────────────────▼─────────────────────────────┐
│  OBSERVER (read-only)                                    │
│  proof034_observation.json                               │
└───────────────────────────┬─────────────────────────────┘
                            │ gates
┌───────────────────────────▼─────────────────────────────┐
│  RELEASE (unchanged by this audit)                       │
│  gate_engine.py → proof034_complete                      │
└─────────────────────────────────────────────────────────┘

Parallel (non-substitute):
  SIM-006 fixture replay → proof_replay_hash
  Coq PROOF-018–033 → ESTABLISHED (formal layer)
```

---

*End of canonical authority model.*
