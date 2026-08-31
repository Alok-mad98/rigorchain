# Consensus Kernel Extraction (P0-050)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-EXTRACTION-EXECUTABLE-P0-050`  
**Date:** 2026-08-30  
**Nature:** Coq extraction to an executable OCaml kernel. Not node linkage. Not refinement.

```
REFINEMENT_PROVED              = false
EQUIVALENCE_PROVED             = false
PRODUCTION_CONSENSUS_MODIFIED  = NO
EXTRACTED_CONSENSUS_KERNEL     = OFF (no production switch)
```

ALIGNMENT ≠ EXTRACTION ≠ REFINEMENT ≠ EQUIVALENCE.

---

## Canonical Coq objects (located, not rewritten)

| Kernel surface | Coq definition | Module | Executable |
|---|---|---|---|
| epoch | `issuance_epoch` | `IssuanceHeightBoundary` | yes |
| ORU | `oru_subsidy` | same | yes |
| RES schedule | `get_res_mint_reward` | `ResSchedule` | yes |
| RES actual @0 | `res_issued_at_height` | `CanonicalIssuance` | yes |
| issuance pack | `compute_rigor_issuance` | extraction packaging of the three | yes |
| maturity | `coinbase_mature` / `COINBASE_MATURITY` | `WindowGeneric` | yes |
| classify | `classify` | `AssetClassification` | yes |
| supply apply | `apply_delta` / `genesis_supply` | `MintSupplyConservation` | yes |
| claimable | `coinbase_oru_cap` | `FeeBurn.Accounting` | yes |
| ledger Δ | `ledger_oru_delta` | RFC-002 identity (`subsidy − converge`) | yes |
| RES coinbase | `rigor_res_coinbase_expected` | `fees+bootstrap+mint` | yes |
| draw | `resolve_convergence_draw` | `select_winner` ∘ T-filter | yes |

Phase5 `VACUOUS-PLACEHOLDER` is not an extraction authority.

`GetConvergenceDrawSetForBlock` is not extracted.

---

## Genesis (schedule ≠ actual)

| Fact | Extracted | C++ Kernel |
|---|---|---|
| `oru_subsidy 0` / `GetBlockSubsidy(0)` | 0 | 0 |
| `get_res_mint_reward 0` / `GetResMintReward(0)` | 40000000 | `RES_REWARD_GENESIS` |
| `res_issued_at_height 0` | 0 | ConnectBlock skip (orchestration) |
| empty F | `NONE` | `ResolveConvergenceDraw` none |
| circulating Δ | 0 | skip / zero |

`GetResMintReward(0)` was not set to 0.

---

## Tests / baseline (2026-08-30)

| Check | Result |
|---|---|
| Coq 8.15.0 `ExtractConsensusKernel.v` | PASS (~1s) |
| `ocamlopt` 4.13.1 CLI | PASS (~1s) |
| extracted CLI startup | <10ms |
| OCaml ↔ C++ issuance/epoch/classify/supply/maturity | PASS |
| Σ 1..209999 / 1..210000 ORU atoms | PASS (~0.6s) |
| Negative: empty script, invalid/ordinary mint, empty F | REJECT / INVALID / NONE |
| `run_differential_gate.sh` | PASS |
| `mining_solo_gbt_copy_bytes.py` (P041) | PASS |
| Existing golden files | unchanged |

Per-invocation CLI cost (~2.3s / 1000 process spawns) is process overhead.
In-process `sum-oru 1..210000` is ~0.6s. No `PERFORMANCE_BLOCKER`.
Arithmetic stays on extracted `Z`.

---

## Representation

| Coq | Extracted OCaml |
|---|---|
| `Z` (height, amounts, T, σ) | inductive `z` / `positive` (arbitrary precision) |
| `nat` (script bytes) | `ExtrOcamlNatInt` → `int` |
| lists / records | OCaml lists / records |

No `Extract Constant`. σ and T stay on `Z`.

---

## Integration boundary (P051)

```
extracted consensus_kernel.ml
        ↓
consensus_kernel_cli.ml          (I/O only)
        ↓
C++ Kernel API                   (production path, unchanged)
        ↓
ConnectBlock                     (orchestration)
```

Default production path remains the C++ Kernel.

---

## Why this TASK did not change the protocol

Extraction is a compilation of existing Coq. C++ Kernel is only *called* by an adapter CLI. No W/T/issuance/epoch/maturity/draw/classify change. Golden files unchanged.
