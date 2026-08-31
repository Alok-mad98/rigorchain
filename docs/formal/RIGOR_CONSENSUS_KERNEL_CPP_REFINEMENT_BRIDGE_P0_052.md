# Consensus Kernel Coq ↔ C++ Bridge (P0-052)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-CPP-REFINEMENT-BRIDGE-P0-052`  
**Date:** 2026-08-30  
**Nature:** Explicit object mapping. Not a refinement. Not an equivalence. Not a production switch.

```
COQ_CANONICAL                      = PASS
CPP_KERNEL                         = PASS
BRIDGE_MAPPING                     = PASS
CONSENSUS_KERNEL_BRIDGE_STATUS     = CLOSED
REFINEMENT_PROVED                  = false
EQUIVALENCE_PROVED                 = false
PRODUCTION_CONSENSUS_MODIFIED      = NO
PROTOCOL_PARAMETERS_MODIFIED       = NO
```

This TASK names the Coq object that each C++ Kernel API corresponds to,
records input/output layers, and lists the proofs that are still missing.
It does not prove C++ = Coq.

---

## Mapping

| Surface | Coq | C++ | Status |
|---|---|---|---|
| epoch | `issuance_epoch` | `RigorIssuanceEpoch` | mapped |
| ORU | `oru_subsidy` | `GetBlockSubsidy` | mapped |
| RES schedule | `get_res_mint_reward` | `GetResMintReward` | mapped |
| RES actual @0 | `res_issued_at_height` | ConnectBlock genesis skip | mapped |
| classify | `AssetClassification.classify` | `ClassifyRigorTxout` | mapped |
| draw | `select_winner` / `pick_better_tx` | `ResolveConvergenceDraw` | mapped |
| supply apply | `apply_delta` | `RigorSupplyDelta` | **layer_aligned** |
| ledger Δ | `ledger_oru_delta` | `oru_circulating_delta` | mapped |
| claimable | `coinbase_oru_cap` | `oru_claimable` | mapped |
| RES coinbase | `cpp_res_coinbase_expected_bridge` | `RigorResCoinbaseExpected` | mapped |

`ClassifyRigorSpendPredicate` does not exist.  
`GetConvergenceDrawSetForBlock` is state assembly, not Kernel.  
`BuildCoinbase` is miner/GBT, not Kernel.

---

## Supply layers (not IDENTICAL)

```
C++  oru_circulating_delta  = subsidy − converge     (RFC-002 ledger)
Coq  apply_delta circulating = subsidy − converge − burn  (RFC-018 UTXO)
```

`SUPPLY_LAYER_SEMANTICS = ALIGNED_BY_LAYER`.  
Do not unify these fields.

---

## Genesis (unchanged)

| Fact | Value |
|---|---|
| `GetBlockSubsidy(0)` | 0 |
| `GetResMintReward(0)` | 40000000 (schedule) |
| `res_issued_at_height(0)` | 0 (actual) |
| draw | none |
| circulating Δ | 0 |

`GENESIS_SCHEDULE_VALUE != GENESIS_ACTUAL_ISSUANCE`.

`epoch(h) = floor(h / 210000)`. First halving = 210000.  
ORU maturity = 100. RES maturity = 100.

---

## Missing proofs (registered, not claimed)

- ∀-height refinement of `GetBlockSubsidy`
- `ResolveConvergenceDraw` ↔ `select_winner` on production SHA256 σ
- ∀-byte VST of `ClassifyRigorTxout`
- `RigorSupplyDelta` reject-reason refinement

These keep `REFINEMENT_PROVED = false`.

---

## Coq files

`ConsensusKernelBridge.v`, `SupplyDeltaBridge.v`, `DrawResolverBridge.v`,
`ClassificationBridge.v`, `CoinbaseLayoutBridge.v`.

Aliases of existing objects. No new formula. No `Axiom` / `Parameter` /
`Admitted`. Honesty flags compile to `false`.

---

## Tests

| Check | Role |
|---|---|
| `coqc` on the five bridge modules | compile |
| `test_consensus_kernel_bridge.py` | inventory + OCaml↔C++ heights |
| `rigor_consensus_kernel_bridge_tests.cpp` | C++ pins |
| `run_differential_gate.sh` step 3c | existing gate |

No production consensus source was modified.
