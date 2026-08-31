# Consensus Kernel First Cut (P0-047)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-FIRST-CUT-P0-047`  
**Date:** 2026-08-30  
**Nature:** Production C++ extraction. Semantics unchanged. Coq untouched.

```
PRODUCTION_CONSENSUS_MODIFIED = NO
COQ_MODIFIED                  = NO
equivalence_claimed           = false
refinement_proved             = false
```

This is the intended correspondence for later P049/P050. It is **not** a
refinement proof.

| C++ Kernel function | Intended formal object |
|---|---|
| `ComputeRigorIssuance(height)` / `GetBlockSubsidy` / `GetResMintReward` | `CanonicalIssuance.issuance` / `IssuanceHeightBoundary` / `ResSchedule` |
| `RigorIssuanceEpoch(height)` = `floor(height / 210000)` | same epoch formula (not `(height-1)/210000`) |
| `ResolveConvergenceDraw(set, T)` | future UTXO draw resolver (`F` = `set.candidates`; `σ` computed inside; `T` = target). **Not** `legacy_account_model/Selection.v` |
| `GetConvergenceDrawSetForBlock` | state / window access (not a Coq kernel object) |
| `RigorSupplyDelta(...)` | `MintSupplyConservation.apply_delta` / ConnectBlock monetary slice |
| `ClassifyRigorAsset` / `AssetTypeFromScriptPubKey` / `ClassifyRigorTxout` | `AssetClassification.classify` |
| coinbase scriptSig / RES mint layout (`ParseCoinbaseConvergenceRootScriptSig`, `CoinbaseHasResMint`, `RigorResCoinbaseExpected`, `ConnectEvaluateCoinbaseResMint`) | mint / coinbase canonical layout |

Notes:

- Height 0 **schedule** RES is `W(0)` (`GetResMintReward(0) == RES_REWARD_GENESIS`). Applied genesis mint is 0 because ConnectBlock skips the genesis UTXO. Do not change the schedule to 0.
- PoW Kernel and Transaction Kernel (`VerifyRigorPredicate`, sighash, Ed25519) are out of this cut.
