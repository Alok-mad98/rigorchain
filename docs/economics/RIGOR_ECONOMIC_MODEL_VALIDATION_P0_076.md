# RIGOR Economic Model Validation (P0-076)

**Task:** `TASK-RIGOR-ECONOMIC-MODEL-VALIDATION-PHASE-D-P0-076`  
**Date:** 2026-08-31  
**Scope:** Mainnet economic-model *analysis, simulation, and validation tools*.  
Mainnet is not live. Historical compatibility is out of scope.

```
ECONOMIC_MODEL_VALIDATION     = PASS
CONSENSUS_MODIFIED            = FALSE
ISSUANCE_MODIFIED             = FALSE
FORMULA_DUPLICATION           = FALSE
SECURITY_MODEL_BASELINE       = COMPLETE
```

This validates protocol economic behavior.
It does not prove market price, future adoption, or external economic
equilibrium.

---

## Boundary

| Done | Not done |
|------|----------|
| Frozen-vector issuance simulation | Consensus rule change |
| Circulating / locked / matured accounting | Issuance formula change |
| ORU / RES interaction analysis | RES schedule change |
| Protocol-native security-budget projection | PoW / ASERT change |
| Economic attack-surface classification | Consensus Kernel change |
| Standalone validation test | Second economic-model authority |
| | ConnectBlock path |
| | Wallet / miner / RPC consensus dependency |
| | Copy of `GetBlockSubsidy` / `GetResMintReward` / `RigorSupplyDelta` |

Formula source remains `RigorConsensusKernel`. Simulation reads
`formal_verification/release_consensus_vectors.json` (plus already-frozen
height pins in `genesis_consensus_vectors.json` and
`issuance_height_boundary_p0_040.json`). Unpinned RES epochs are not
invented in Python; `GetResMintReward` is the authority.

---

## Artifacts

| Path | Role |
|------|------|
| `tools/economic_model_simulator.py` | Issuance + supply simulation from frozen vectors |
| `tools/security_budget_projection.py` | 0–10y and 10–50y protocol-native security budget |
| `formal_verification/economic_supply_model.json` | Supply snapshots + invariants |
| `formal_verification/economic_model_validation_manifest.json` | Audit manifest |
| `docs/economics/RIGOR_ORU_RES_ECONOMIC_MODEL.md` | ORU / RES interaction |
| `docs/security/RIGOR_ECONOMIC_ATTACK_SURFACE_P0_076.md` | Attack-surface classification |
| `core/src/test/test_economic_model_validation.cpp` | Standalone C++ pins (not the `test_rigor` consensus path) |

---

## 1. Issuance schedule

Required heights: `0, 1, 99, 100, 101, 210000, 420000, 840000`, plus a 50-year
horizon (`2,629,800` blocks at 600 s spacing).

| height | epoch | ORU / block | RES schedule (frozen pin) |
|-------:|------:|------------:|---------------------------|
| 0 | 0 | 0 | 40.000000 RES |
| 1 … 101 | 0 | 1000 ORU | 40.000000 RES |
| 210000 | 1 | 500 ORU | 14.142196 RES |
| 420000 | 2 | 250 ORU | 7.698004 RES |
| 840000 | 4 | 125 ORU (last pinned floor) | Kernel `GetResMintReward` (not in release vectors) |

Height 0 is not a premine. Schedule is not actual RES issuance.

---

## 2. Supply model

`formal_verification/economic_supply_model.json` carries:

```
total_supply, circulating_supply, locked_supply,
matured_supply, burned_supply, minted_res_supply
```

Baseline (no convergence burns, empty draw ⇒ no RES mint):

| Scenario | height | circulating ≤ total | locked + circulating + burned = issued |
|----------|-------:|---------------------|----------------------------------------|
| genesis | 0 | yes (0 ≤ 0) | yes |
| early mining | 1 | yes (0 ≤ 1000 ORU) | yes (all locked) |
| maturity release | 101 | yes | yes (first coinbase matures) |
| halving epoch 1 | 210000 | yes | yes |
| halving epoch 2 | 420000 | yes | yes |
| long horizon | 2629800 | yes | yes |

`minted_res_supply = 0` on this baseline because `M(h)=0`. Scheduled RES is
a separate diagnostic field and is not added to ORU `total_supply`.

---

## 3. ORU / RES interaction

See [RIGOR_ORU_RES_ECONOMIC_MODEL.md](./RIGOR_ORU_RES_ECONOMIC_MODEL.md).

RES does not cause hidden inflation, duplicate issuance, or a supply
accounting mismatch: schedule is authorization; mint is `M(h)∈{0,1}` of
size `GetResMintReward(h)`; ORU and RES are different assets.

---

## 4. Security budget

`tools/security_budget_projection.py` reports annual ORU issuance, tip miner
reward, inflation versus circulating supply, and protocol-native security
expenditure. No price.

| Horizon | Tip subsidy | Annual issuance (atoms) | Inflation vs circulating |
|---------|-------------|-------------------------|--------------------------|
| year 10 | 250 ORU | 1,314,900,000,000,000 | ~4.00% |
| year 50 | 125 ORU | 657,450,000,000,000 | ~1.08% |

The tail is strictly positive. This is not a hashprice or fiat security
proof.

---

## 5. Attack surface

See [RIGOR_ECONOMIC_ATTACK_SURFACE_P0_076.md](../security/RIGOR_ECONOMIC_ATTACK_SURFACE_P0_076.md).

| Attack | Risk |
|--------|------|
| Early supply concentration | MEDIUM |
| Maturity manipulation | LOW |
| RES timing attack | MEDIUM |
| Draw gaming incentive | MEDIUM |
| Miner exit pressure | MEDIUM |
| Emission shock | LOW |

HIGH findings: none.

---

## 6. Tests

`test_economic_model_validation.cpp` is standalone. It calls
`GetBlockSubsidy`, `GetResMintReward`, `ComputeRigorIssuance`, and
`RigorSupplyDelta`. It does not enter ConnectBlock and is not wired into
the `test_rigor` consensus suites.

Coverage: genesis supply, maturity 99/100/101, halving 210000/420000/840000,
RES schedule vs actual mint, circulating Δ, overflow reject.

---

## 7. Manifest

`formal_verification/economic_model_validation_manifest.json`:

```
consensus_formula_source = RigorConsensusKernel
economic_model           = simulation_only
consensus_modified       = false
formula_duplicate        = false
```

---

## 8. Gates

| Command | Result |
|---------|--------|
| `python3 tools/economic_model_simulator.py` | `ECONOMIC_MODEL_SIMULATION=PASS` |
| `python3 tools/security_budget_projection.py` | `SECURITY_BUDGET_PROJECTION=PASS` |
| `python3 tools/lint-consensus-authority.py` | `AUTHORITY_LOCK=PASS` / `PRODUCTION_CONSENSUS_MODIFIED=NO` |
| `SKIP_TEST_RIGOR=1 bash formal_verification/scripts/run_differential_gate.sh` | `differential gate: PASS` |
| standalone `test_economic_model_validation` | `PASS: economic model validation` |
| `./core/src/test/test_rigor` | 1109 cases, no errors (regression; this TASK does not add a consensus suite) |

Reproduce the C++ pin binary:

```bash
g++ -O2 -std=c++17 -ffunction-sections -fdata-sections -Icore/src \
  -o formal_verification/extracted/test_economic_model_validation \
  core/src/test/test_economic_model_validation.cpp \
  core/src/consensus/rigor_issuance.cpp \
  core/src/consensus/rigor_supply.cpp \
  tools/consensus_kernel_refinement_log_stub.cpp \
  core/src/uint256.cpp \
  core/src/util/strencodings.cpp \
  -Wl,--gc-sections
formal_verification/extracted/test_economic_model_validation
```

---

## Final status

```
ECONOMIC_MODEL_VALIDATION = PASS
CONSENSUS_MODIFIED        = FALSE
ISSUANCE_MODIFIED         = FALSE
FORMULA_DUPLICATION       = FALSE
SECURITY_MODEL_BASELINE   = COMPLETE
```

This validates protocol economic behavior.
It does not prove market price, future adoption, or external economic
equilibrium.
