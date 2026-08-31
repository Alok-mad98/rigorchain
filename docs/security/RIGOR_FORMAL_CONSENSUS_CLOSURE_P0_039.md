# Formal consensus closure (P0-039)

**Task:** `TASK-RIGOR-FORMAL-CONSENSUS-CLOSURE-P0-039`  
**Date:** 2026-08-29

This is a breakpoint closure, not a catalog. Production W / T / OpuHash / RES schedule / maturity were not changed. Script VM was not restored. `refinement_proved` stays **false**.

```
equivalence_claimed              = false
classifier_refinement_proved     = false
connectblock_refinement_proved   = false
cpp_refinement_proved            = false
```

## What closed

**Classifier.** Live Coq `classify` now uses the unique `GetResMintScript` parser (`parse_canonical_mint`). `OP_PUSHBYTES_1 0x05` is RES; `OP_5` is INVALID. `classify_cpp_aligned := classify`. Finite golden vectors (C++ `ClassifyRigorTxout` + Python `asset_from_script` + Coq `classify`) include those two encodings plus v0/20, v1/32, RES_MINT, CONVERGE, OP_RETURN, INVALID. That is **not** `∀ spk. C++ = Coq`.

**ConnectBlock (model).** `apply_block_supply` is the monetary slice:

```
circulating' = circulating + subsidy + valid_mint − converge − fee_burn
```

Qed (model): invalid tx / unauthorized mint rejected; ordinary tx cannot mint; immature mint cannot spend; orphan mint is not a summand of the surviving chain. C++ `TestChain100Setup` checks genesis subsidy 0, height-1 1000, observational ORU ledger = Σ subsidy − converge, genesis UTXO absent. That is **not** `ConnectBlock = apply_block_supply`.

**Reorg issuance.** Surviving-chain RES ≤ schedule at depth 1, W, >W, and >100. W does not enter subsidy, T, or maturity. Reward is height-only.

**Amount boundary.** In-range `CAmount` sums (`≤ MAX_MONEY`) stay inside int64; circulating delta does not wrap. C++ `AddMoneyOverflows(MAX_MONEY, 1)`.

**Fuzz (counterexample search, not theorems).** Classifier/mint uniqueness; sighash via existing `script_interpreter`; OpuHash on header ids in `pow`; supply/reorg deltas including depth 100; RIGORCAP unknown bits cannot enable.

## Explicitly not proved

- VST of `validation.cpp` / libsodium Ed25519
- `∀` byte-string equality of `ClassifyRigorTxout` and `classify`
- `ActivateBestChain` = Coq reorg
- P2P capability as a consensus predicate (it is not)

Evidence: `formal_verification/rigor_v01/{AssetClassification,ClassifierRefinementBoundary,ConnectBlockSupply,ReorgIssuanceBound,AmountBoundary,VectorAgreement}.v`, golden JSON, `core/src/test/mint_supply_reorg_p0_036_tests.cpp`, `rigor_classification_vector_tests`.
