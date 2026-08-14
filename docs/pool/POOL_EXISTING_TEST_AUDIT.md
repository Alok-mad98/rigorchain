# POOL_EXISTING_TEST_AUDIT

**Task:** `TASK-POOL-ARCHITECTURE-AUDIT-001` · Phase 5  
**Date:** 2026-08-12  
**Scope:** Inventory existing local / closed-testnet tests related to pool, miner, RPC, block submission. Read-only.

---

## 1. Summary

| Area | Coverage | Maturity |
|------|----------|----------|
| Rust unit / integration (pool crates) | **Strong** (~223 `#[test]` attributes) | Alpha CI-capable |
| Pilot E2E simulation | **Present** | Fixture / in-process |
| Live `rigord` submitblock acceptance | **Partial** | Manual / scripts; not full CI gate |
| Stock cpuminer ↔ Stratum | **Weak** | Agent UA gate historically blocked |
| Node C++ miner / GBT functional tests | **Present** (core) | Consensus side |
| Legacy yiimp PHP/C++ tests | **Irrelevant** for RIGOR | Do not run as RIGOR proof |

Prior SECURITY-001-A5 report claimed **184 pass / 0 fail** across pool crates at that time; attribute counts below are a static inventory (not a fresh full `cargo test` run in this audit).

---

## 2. Pool crate tests (static inventory)

| Crate | Approx. `#[test]` attrs | Primary test files | What they cover |
|-------|-------------------------|--------------------|-----------------|
| `stratum-server` | 57 | `tests/integration.rs` + module tests | Protocol, orchestrator, mint wiring, PPLNS, winner |
| `mint-engine` | 35 | `tests/integration.rs`, `src/engine.rs`, `res_owner.rs` | Body mint build, owner declaration, assemble |
| `rigor-gbt` | 24 | `tests/integration.rs`, `validate.rs` | GBT normalize / validation |
| `block-assembler` | 23 | `tests/integration.rs`, `tests/convergence.rs` | Assembly, placeholder, commitments |
| `rigor-miner-agent` | 20 | `tests/integration.rs`, `messages.rs`, `mining_scanner.rs`, `opuhash.rs` | Stratum client, OpuHash scan |
| `share-validator` | 17 | `tests/integration.rs` | Share/block validation pipeline |
| `winner-engine` | 17 | `tests/integration.rs`, `service_integration.rs`, `golden_*.rs` | Draw, authorize, persist golden |
| `shared/metrics` | 12 | crate tests | Telemetry schema |
| `pilot` | 10 | `tests/e2e_simulation.rs`, `stability_021.rs` | In-process stack simulation |
| `rigor-convergence-root` | 4 | `tests/golden_cr.rs`, `live_root_replay_122.rs`, `display_encoding.rs` | Root encoding / replay |
| `rigor-pool-pg` | 4 | `tests/backend_memory.rs` | Memory backend smoke |

**How to run (operator):** from each crate directory, `cargo test` (or workspace members as configured). Full-node `make` / `make check` remains operator-run per project convention.

---

## 3. Pilot / ops scripts

| Path | Role |
|------|------|
| `contrib/testing/pool_pilot/devtest_stratum_miner.py` | Devtest Stratum miner client |
| `contrib/testing/pool_pilot/run_devtest_pilot.sh` | Pilot runner |
| `contrib/testing/pool_pilot/run_pilot_simulation.sh` | Simulation |
| `contrib/testing/pool_pilot/simulate_reorg.sh` | Reorg exercise |
| `contrib/testing/pool_pilot/verify_pilot_artifacts.py` | Artifact checks |
| `contrib/testing/pool_pilot/audit_mining_021.py` | Stability audit helper |
| `contrib/alpha/validate_pool_local.sh` | Local alpha pool validation |
| `scripts/pool_pilot_closed_testnet.sh` | Closed-testnet pilot |
| `docs/mining/POOL_CLOSED_TESTNET_PILOT.md` | Procedure |

---

## 4. Miner tests

| Path | Role |
|------|------|
| `pool/rigor-miner-agent/tests/*` | Stratum agent unit/integration |
| `cpuminer/` | Solo miner; selftest hooks in C sources |
| `contrib/alpha/validate_miner_local.sh` | Local miner validation script |
| `docs/release/MINER_LOCAL_VALIDATION_REPORT.md` | Prior miner validation report |
| `core/src/test/miner_tests.cpp` | Node miner unit tests (not pool) |

---

## 5. RPC / GBT / block submission tests

| Path | Role |
|------|------|
| `pool/rigor-gbt/tests/integration.rs` | GBT adapter integration |
| `pool/mint-engine/src/rpc.rs` + tests | `submitblock` / RPC client surfaces |
| `pool/mint-engine/src/mempool_preflight.rs` | `testmempoolaccept` preflight |
| `pool/stratum-server/tests/integration.rs` | Block candidate → mint → submit sink (mock/in-process) |
| `core/test/functional/feature_dual_pool_mining.py` | Node dual-pool mining functional (mempool domain; name “pool” ≠ Stratum) |
| `core/doc/mining-pool-gbt.md` | GBT contract (doc) |

**Gap:** Continuous CI proof of **live** `submitblock` acceptance against a real `rigord` tip for the full Stratum→mint path is still **PARTIAL** (pilot/manual evidence exists; not a hard public-readiness gate).

---

## 6. What is missing before public pool

| Missing test class | Why it matters |
|--------------------|----------------|
| Multi-miner soak under PG persistence | Share durability + payout correctness |
| TLS / reconnect / HA failover | Spec’d in TASK-MINING-023A; not proven |
| Production treasury UTXO lifecycle | Mint funding exhaustion / double-spend |
| RES-boundary negative tests | Assert pool **cannot** mint/allocate if policy hardens |
| Stock miner compatibility matrix | cpuminer / third-party agents |
| Confirmation-depth payout finality | Deep reorg beyond unit fixtures |

---

## 7. Acceptance (Phase 5)

| Criterion | Result |
|-----------|--------|
| Existing pool / miner / RPC / submit tests identified | **PASS** |
| Gaps documented | **PASS** |
| No code changes | **PASS** |
