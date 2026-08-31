# Consensus Kernel Coq ↔ C++ Alignment (P0-049)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-COQ-CPP-ALIGNMENT-P0-049`  
**Date:** 2026-08-30  
**Nature:** Semantic alignment audit. Not extraction. Not refinement.

```
REFINEMENT_PROVED   = false
EQUIVALENCE_PROVED  = false
equivalence_claimed = false
CPP_PRODUCTION_MODIFIED = NO
```

ALIGNMENT ≠ differential agreement ≠ refinement ≠ equivalence.

---

## Genesis / schedule (do not conflate)

| Concept | Value | Authority |
|---|---|---|
| `GetResMintReward(0)` / `get_res_mint_reward 0` | `RES_REWARD_GENESIS` (40 RES) | schedule |
| `GetBlockSubsidy(0)` / `oru_subsidy 0` | 0 | schedule = actual |
| ConnectBlock genesis | skip txs; empty F; no UTXO apply | actual issuance |
| `res_issued_at_height 0` | 0 | actual issuance model |
| Circulating Δ at genesis | 0 ORU, 0 RES | actual |

Coq **does** distinguish schedule from actual issuance
(`CanonicalIssuance.res_issued_at_height 0 = 0` while
`get_res_mint_reward 0 = W(0)`).  
`GENESIS_SCHEDULE_VS_ISSUANCE_MODEL_GAP` is **not** raised.

This TASK did not set `GetResMintReward(0)` to 0.

---

## Issuance / maturity / draw / circulating (keep separate)

```
ISSUANCE            schedule(h) / GetBlockSubsidy / GetResMintReward
≠ ACTUAL ISSUANCE   ConnectBlock application / res_issued_at_height
≠ MATURED SUPPLY    spend_height - coin_height >= 100
≠ DRAW ELIGIBLE     F assembled from mature converge tickets in W
≠ CIRCULATING       RFC-002 ledger Δ = subsidy − C(t)
```

ORU maturity = 100. RES mint maturity = 100.  
Maturity does not change the schedule, the epoch, or genesis issuance.

Height 1 is the first ORU issuance. That coinbase is immature at height 100
and mature at height 101. Genesis produces no maturity source (no connected
UTXO, no ORU, no F).

---

## Alignment matrix

| Object | Coq | C++ | Status | Evidence |
|---|---|---|---|---|
| epoch | `issuance_epoch h = h/210000` | `RigorIssuanceEpoch` | ALIGNED | EVIDENCE |
| ORU issuance | `oru_subsidy` | `GetBlockSubsidy` | ALIGNED | EVIDENCE |
| RES schedule | `get_res_mint_reward` | `GetResMintReward` | ALIGNED | EVIDENCE |
| Genesis ORU actual | 0 | 0 + ConnectBlock skip | ALIGNED | EVIDENCE |
| Genesis RES actual | `res_issued_at_height 0 = 0` | skip | ALIGNED | EVIDENCE |
| Genesis RES schedule | W(0) | W(0) | ALIGNED | EVIDENCE |
| Genesis draw | `select_winner [] = None` | empty F → none | ALIGNED | EVIDENCE |
| Maturity | 100 / 100 | 100 / 100 | ALIGNED | EVIDENCE |
| Draw resolver | `select_winner` / `pick_better_tx` | `ResolveConvergenceDraw` | ALIGNED | EVIDENCE |
| Classify | `AssetClassification.classify` | `ClassifyRigorTxout` | ALIGNED | EVIDENCE |
| Supply Δ | `apply_delta` / `coinbase_oru_cap` / ledger Δ | `RigorSupplyDelta` | ALIGNED | STRUCTURAL |
| Coinbase layout | mint shape; no scriptSig twin | predicates only | ALIGNED | STRUCTURAL |

Phase5Equivalence `VACUOUS-PLACEHOLDER` / `forall s, True` / `exact I` is
**BOOKKEEPING**. It is not alignment evidence.

`ScriptMintWitness.asset_from_script` is a replay shape model. Production
classifier authority on the Coq side for golden/extraction is
`AssetClassification.classify`. Not a second C++ matcher.

---

## Draw boundary

```
state
  ↓
GetConvergenceDrawSetForBlock     (orchestration; not Kernel)
  ↓
F
  ↓
ResolveConvergenceDraw(F, T)      ↔  select_winner (eligible F) (T)
  ↓
winner | none
```

Genesis records `source_height = 0` (`IsSet` is true) and `candidates = []`.
`ResolveConvergenceDraw` returns no winner. Height 0 cannot start
ORU → draw → RES.

---

## Supply field mapping

| C++ `RigorSupplyDelta` | Coq | Notes |
|---|---|---|
| `oru_subsidy` | `oru_subsidy` / `cb_expected_oru_subsidy` | same schedule |
| `res_reward_scheduled` | `get_res_mint_reward` | schedule, not actual |
| `oru_claimable` | `coinbase_oru_cap` | `(fees − burn) + subsidy` |
| `oru_circulating_delta` | `ledger_oru_delta` = subsidy − converge | RFC-002; name is “circulating” in C++ |
| Coq `circulating_oru_delta` | subsidy − converge − burn | RFC-018 UTXO view; not a Kernel output field |
| `res_delta` | `bd_valid_mint` | accepted mint |
| genesis application | zero `BlockDelta` / `res_issued_at_height 0` | ConnectBlock skip |

No second production supply formula.

---

## Why this TASK did not change the protocol

No production-semantic discrepancy was found. Changing `GetResMintReward(0)`,
W, T, epoch, draw, classify, or ConnectBlock genesis skip would be a protocol
change. P049 only records the mapping.

P048 isolation items (mixed draw TU, classify includes `predicate_verify.h`,
coinbase header names `Build*`, `LogPrintf`) remain **NOT_BLOCKING_FINDING**.

---

## P050 boundary

Next, if this alignment stays closed:

```
Coq canonical Kernel  →  Extraction  →  executable kernel  →  node boundary
```

Do not treat this document, the JSON matrix, or golden agreement as
`refinement_proved` or `equivalence_proved`.

---

## Tests (2026-08-30)

| Check | Result |
|---|---|
| `coqc` `ConsensusKernelAlignment.v` (8.15.0) | PASS |
| `audit_assumptions.py --strict` on that module | 21 AXIOM_FREE; 0 Axiom/Admitted/Parameter |
| `test_consensus_kernel_alignment.py` | PASS |
| `test_coqproject_partition.py` | PASS |
| `test_rigor --run_test=rigor_consensus_kernel_tests` | PASS (11 cases) |
| `run_differential_gate.sh` | PASS |
| Existing golden vector files | unchanged |

---

## Verdict

`FINAL_VERDICT = CONSENSUS_KERNEL_ALIGNMENT_CLOSED`

`REFINEMENT_PROVED = false`  
`EQUIVALENCE_PROVED = false`
