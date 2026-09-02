# Consensus Kernel Formal Refinement Boundary (P0-056)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-FORMAL-REFINEMENT-P0-056`  
**Date:** 2026-08-30  
**Nature:** Kernel-boundary ∀-refinement in Coq. Not a full-node proof.

```
CONSENSUS_KERNEL_REFINEMENT_STATUS = CLOSED
ISSUANCE_REFINEMENT                = PASS
CLASSIFICATION_REFINEMENT          = PASS
DRAW_REFINEMENT                    = PASS
SUPPLY_DELTA_REFINEMENT            = PASS
COINBASE_BOUNDARY_REFINEMENT       = PASS
COQ_ADMITTED                       = 0
COQ_AXIOMS                         = 0
COQ_PARAMETERS                     = 0
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
PRODUCTION_CONSENSUS_MODIFIED      = NO
COQ_MODEL_CLOSURE                  = PROVED
COQ_TO_CPP                         = VECTOR-TESTED
EXTRACTED_TO_BINARY                = NOT-LINKED
EQUIVALENCE_PROVED                 = FALSE
```

This is **not** CompCert, VST, ConnectBlock, or Bitcoin full validation.
`Print Assumptions` on the five kernel theorems is
`Closed under the global context`.

P053 finite-vector JSON flags stay `refinement_proved = false`
(execution compare ≠ this ∀-file).

---

## Proven

| Surface | C++-shaped Coq | Canonical Coq | Theorem |
|---|---|---|---|
| Issuance | `cpp_RigorIssuanceEpoch`, `cpp_GetBlockSubsidy`, `cpp_GetResMintReward`, `cpp_issuance` | `issuance_epoch`, `oru_subsidy`, `get_res_mint_reward`, `coq_issuance` | `issuance_refinement` |
| Classification | `cpp_ClassifyRigorTxout` → `cpp_ClassifyRigorAsset` | `AssetClassification.classify` | `classification_refinement` |
| Draw | `cpp_ResolveConvergenceDraw` | `select_winner` after eligibility filter | `draw_refinement` |
| Supply | `RigorSupplyDelta_cpp` | `ConsensusLedgerDelta` = `ledger_oru_delta` | `supply_delta_refinement` |
| Coinbase | `expected_res_coinbase_cpp` | `expected_res_coinbase_coq` | `coinbase_boundary_refinement` |

Issuance is proved for `kernel_params_frozen params` and `0 <= h`
(production heights). Pins include
`{0, 1, 99, 100, 209999, 210000, 419999}`.

`GetResMintReward(0) = 40000000` (schedule).
`res_issued_at_height 0 = 0` (actual). They are not merged.

Classification is `forall spk : list byte`. Fail-closed on empty,
unknown, and malformed witness. No sighash / Ed25519 / predicate_verify.

Draw is `ResolveConvergenceDraw(F,T)` only.
`GetConvergenceDrawSetForBlock` is not in the theorem.
512-bit cross-product is `cpp_mul512 = Z.mul`.
Exact σ-product ties break on `bytes_ltb` txid
(`draw_txid_tie_break`). Covered: empty F, one candidate, weight 0, T=0.

Supply proves

```
RigorSupplyDelta_cpp = ConsensusLedgerDelta = ledger_oru_delta_coq
```

where `ConsensusLedgerDelta = subsidy − converge`.
Fee burn is **not** in this theorem
(`fee_burn_not_in_ledger_delta`).

Coinbase proves `fees + bootstrap + mint` only.
`BuildCoinbaseCommitmentScriptSig` and miner are out of scope.

---

## Not proven

- ConnectBlock refinement
- UTXO mutation refinement
- DB refinement
- PoW refinement
- Transaction Kernel refinement
- Node / binary equivalence (`EQUIVALENCE_PROVED = false`)

---

## Failure policy

If C++-shaped Coq and canonical Coq disagree on the same kernel input:
print `CONSENSUS_KERNEL_REFINEMENT_DISCREPANCY_FOUND` and **STOP**.
Do not edit production C++. Do not edit the Coq formula.

---

## FINAL_VERDICT

`CONSENSUS_KERNEL_FORMAL_REFINEMENT_CLOSED`
