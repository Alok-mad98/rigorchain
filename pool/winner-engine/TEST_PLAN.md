# Winner Engine Test Plan

**Task:** TASK-MINING-016, TASK-MINING-018  
**Spec:** `docs/mining/WINNER_ENGINE_PRODUCTION_SPEC.md`

## Test matrix

| ID | Category | File | Vectors / cases | Status |
|----|----------|------|-----------------|--------|
| T-01 | Golden ConvergenceRoot | `tests/golden_vectors.rs` | CR-000..CR-011 | PASS |
| T-02 | Golden SigmaCR01 | `tests/golden_vectors.rs` | SIG-000..SIG-004 | PASS |
| T-03 | Golden Inequality | `tests/golden_vectors.rs` | INQ-000..INQ-003 | PASS |
| T-04 | Golden RunSelection | `tests/golden_vectors.rs` | WIN-000..WIN-006 | PASS |
| T-05 | Candidate extraction | `tests/integration.rs` | GBT order preserved | PASS |
| T-06 | RFC-015 singleton | `tests/integration.rs` | WIN-002 \|F\|=1 | PASS |
| T-07 | Authorization | `tests/integration.rs` | AUTH happy path | PASS |
| T-08 | Reorg invalidate | `tests/integration.rs` + `reorg.rs` | STALE / REVOKED | PASS |
| T-09 | Display hex roundtrip | `src/hash.rs` | unit | PASS |
| T-10 | Equivalence flag | `tests/golden_vectors.rs` | `EQUIVALENCE_CLAIMED == false` | PASS |
| T-11 | Golden persist memory | `tests/golden_persist.rs` | RunSelection roundtrip | PASS |
| T-12 | Golden persist PostgreSQL | `tests/golden_persist.rs` | `DATABASE_URL` soak | IGNORE |
| T-13 | WinnerRuntime service | `tests/service_integration.rs` | preview → register → authorize → invalidate | PASS |
| T-14 | `preview_winner` alias | `src/engine.rs` unit | delegates to draw replay | PASS |

## Golden vector source

`formal_verification/phase3_golden_vectors.json`  
Tag: `mainnet-freeze-rfc015-2026-05-26`

## P-03 certification gate (not yet closed)

Full `equivalence_claimed = true` requires:

1. All phase3 golden modules PASS (T-01..T-04) — **done in this crate**
2. Live `getconvergenceresult` cross-check on connected blocks — **future soak (M-11)**
3. Pool PostgreSQL soak on live testnet — **TASK-MINING-017 adapters wired; soak pending**

## Commands

```bash
cd pool/winner-engine
cargo test
cargo test --test golden_vectors
cargo test --test integration
```

## Required output table (TASK-MINING-016)

| Field | Status |
|-------|--------|
| Winner Engine Workspace | **COMPLETE** |
| Candidate Extraction | **COMPLETE** |
| Convergence Root Calculation | **COMPLETE** |
| Draw Replay | **COMPLETE** |
| Mint Authorization Token | **COMPLETE** |
| Reorg Handling | **COMPLETE** |
| Persistence Interfaces | **COMPLETE** |
| Golden Tests | **COMPLETE** |
| Integration Tests | **COMPLETE** |
| Documentation | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |

## Required output table (TASK-MINING-018)

| Field | Status |
|-------|--------|
| Stratum Winner Engine Wiring | **COMPLETE** (consumer: `stratum-server`) |
| Mint Engine Authorization Check | **COMPLETE** (consumer: `mint-engine`) |
| preview_winner Replacement | **COMPLETE** |
| RES Mint Authorization Integration | **COMPLETE** |
| Reorg / Invalidation Handling | **COMPLETE** |
| Golden Test Verification | **COMPLETE** (P-03 vectors + WIN-002 mint path) |
| Postgres Persistence Usage | **COMPLETE** (`WinnerRuntime::open`, optional soak T-12) |
| Unit / Integration Tests | **COMPLETE** |
| Documentation | **COMPLETE** |
| Architecture / Consensus / Protocol / Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |
