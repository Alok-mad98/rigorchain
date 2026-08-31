# RIGOR Formal Closure — Mint, Supply, Reorg (P0-036)

**Task:** `TASK-RIGOR-FORMAL-CLOSURE-MINT-SUPPLY-AND-REORG-P0-036`  
**Mode:** Formal model + C++ boundary + negative theorems + fuzz evidence.  
**Date:** 2026-08-29  
**Companion:** [P0-035](./RIGOR_FORMAL_VERIFICATION_PROGRESSIVE_CLOSURE_AUDIT_P0_035.md)

Production consensus C++ was **not** modified.

```
COQ_ADMITTED                        = 0
COQ_AXIOMS                          = 0
COQ_PARAMETERS                      = 0

W32                                 = UNCHANGED
T                                   = UNCHANGED
RES_REWARD                          = UNCHANGED
OPUSHASH                            = UNCHANGED
MATURITY                            = UNCHANGED
CONSENSUS_CPP_MODIFIED              = NO

CPP_REFINEMENT                      = NOT_PROVED
CLASSIFIER_REFINEMENT               = PROOF_OBLIGATION_DEFINED
classifier_refinement_proved        = false
connectblock_refinement_proved      = false
```

Fuzz is **counterexample search**, not a Coq theorem.

---

## What landed

| File | Role |
|------|------|
| `formal_verification/rigor_v01/MintSupplyConservation.v` | Unified `total_circulating' = total + subsidy + mint − converge − fee_burn` |
| `formal_verification/rigor_v01/ConnectBlockSupply.v` | Abstract ConnectBlock monetary slice + named refinement obligation |
| `formal_verification/rigor_v01/ReorgIssuanceBound.v` | Surviving-chain issuance ≤ schedule; orphan mint not in canonical sum |
| `formal_verification/rigor_v01/CanonicalMintEncoding.v` | Unique mint encoder/parser; malformed fail-closed |
| `formal_verification/rigor_v01/ClassifierRefinementBoundary.v` | `classify_cpp_aligned` vs future C++ oracle |
| `core/src/test/fuzz/rigor_scriptpubkey.cpp` | Classifier / mint uniqueness on random scripts |
| `core/src/test/fuzz/rigor_mint.cpp` | Canonical mint round-trip; PUSHDATA / trailing reject |
| `core/src/test/fuzz/rigor_supply.cpp` | CAmount / MAX_MONEY / fee-burn nonnegativity |
| `core/src/test/fuzz/rigor_reorg.cpp` | Snapshot reorg: orphan suffix not in new circulating |
| `core/src/test/mint_supply_reorg_p0_036_tests.cpp` | BOOST pins for frozen params + unique mint + identity |

Honesty: this is **not** `ConnectBlock = apply_block_supply`. DualPool / fee-burn / `ConnectBlock/ConnectBlockSupply.v` remain as prior models; P0-036 is the **unified rigor_v01 state**.

---

## P1 — Mint supply conservation

State:

```
ledger_ORU'      = ledger_ORU + subsidy − converge
circulating_ORU' = circulating_ORU + subsidy − converge − fee_burn
RES'             = RES + valid_mint
```

`total_circulating = circulating_ORU + RES`.

Qed (model): `mint_supply_conservation`, `replay_conservation`, `ordinary_tx_cannot_mint`, `invalid_tx_cannot_enter_supply`, `invalid_mint_cannot_enter_supply`, `immature_mint_cannot_spend`, `fee_burn_does_not_enter_circulating`, `converge_reduces_oru`.

Invalid tx / unauthorized mint: `apply_delta = None` (no state mutation).

---

## P2 — ConnectBlock unified supply

`apply_block_supply = apply_delta ∘ abstract_to_delta`.

Fields locked into the abstract block: subsidy, mint, converge, fee burn, ordinary tx, invalid tx, maturity (still 100), total supply.

```
ConnectBlock_supply_obligation(cpp) :=
  ∀ s b. cpp s b = apply_block_supply s b
```

`cpp_refinement_proved = false`. No Coq `Parameter` / `Axiom`.

---

## P3 — Reorg issuance bound

Canonical RES after a successful chain is `us_res s0 + chain_mint surviving`.  
`chain_mint ≤ chain_mint_cap` when every block is `mint_bounded`.

Qed: `reorg_cannot_create_supply`, depth-1 / depth-W / depth>W specializations, `orphan_mint_cannot_enter_canonical_supply` (orphaned block `~ In` surviving list), `orphan_coinbase_cannot_enter_canonical_supply` (ledger = subsidy − converge of survivors).

Window geometry is still P0-032 (`window_of_source` unchanged by branch). Reward is height-only: different `contents_at` does not change `get_res_mint_reward h`.

This is **not** `ActivateBestChain` / `DisconnectBlock`.

---

## P4 — W / T / reward (frozen instance)

Re-uses P0-034 `T(W)`. Production `W = 32`.

Qed: `W_does_not_change_subsidy`, `W_does_not_change_maturity`, `T_uses_correct_W`, `wrong_W_T_pair_is_not_equivalent` (`~ target_matches_window 36 (T_of_W 32)`), `reward_not_recomputed_from_window`.

---

## P5 — Canonical mint / ScriptPubKey

`parse_canonical_mint` is the inverse of `encode_canonical_mint` (C++ `GetResMintScript` shape: minimal CScriptNum + PUSHBYTES_20 + marker, no trailing).

Qed: round-trips at 0 / 5 / 100 / 150; `parse_canonical_mint_unique`; `two_encodings_cannot_be_same_canonical_mint`; PUSHDATA1 / non-minimal small push / trailing / wrong marker / wrong pkh length → `None`; witness wrong length / trailing → `AssetInvalid`.

Live `classify` still uses a locktime **shape family** (P0-023). Aligned parser is `classify_cpp_aligned`.

---

## P6 — Classifier refinement obligation

```
classifier_refinement_obligation(cpp) :=
  ∀ spk. cpp spk = classify_cpp_aligned spk
```

`CLASSIFIER_REFINEMENT = PROOF_OBLIGATION_DEFINED`  
`classifier_refinement_proved = false`

---

## P7 — Fuzz (evidence, not theorems)

| Target | Looks for |
|--------|-----------|
| `rigor_scriptpubkey` | ParseResMintScript ⇒ unique GetResMintScript; else not RES_MINT |
| `rigor_mint` | Two encodings / trailing / PUSHDATA1 |
| `rigor_supply` | Overflow, negative burn, wrap of subsidy+mint−converge−burn |
| `rigor_reorg` | Depths 1, 31, 32, 33, 64, 288 on a snapshot stack |

---

## P8 — Negative theorems (index)

| Theorem | File |
|---------|------|
| `ordinary_tx_cannot_mint` | MintSupplyConservation |
| `invalid_mint_cannot_enter_supply` | MintSupplyConservation |
| `invalid_tx_cannot_enter_supply` | MintSupplyConservation |
| `immature_mint_cannot_spend` | MintSupplyConservation |
| `orphan_mint_cannot_enter_canonical_supply` | ReorgIssuanceBound |
| `malformed_script_cannot_classify` | ClassifierRefinementBoundary |
| `ambiguous_script_cannot_classify` | CanonicalMintEncoding |
| `reorg_cannot_create_supply` | ReorgIssuanceBound |
| `wrong_W_T_pair_is_not_equivalent` | ReorgIssuanceBound |

---

## Verification

| Check | Result |
|-------|--------|
| `make -f Makefile.coq clean && make -f Makefile.coq -j$(nproc)` | **PASS** (exit 0; includes the five P0-036 modules) |
| Incremental recompile after aligning locktime 1–16 to C++ `<< getvch()` | **PASS** |
| Comment-stripped `Admitted.` / `Axiom` / `Parameter` in `rigor_v01/` | **0** |
| `cd core && make -j$(nproc) && make check` | **PASS** (exit 0) |
| `test_rigor --run_test=mint_supply_reorg_p0_036_tests` | **PASS** (4 cases) |
| `window_parameterization_tests` / `w32_reorg_stability_*` / `script_asset_ownership_freeze_tests` / `res_orum_tests` | **ran in `make check`** |
| Fuzz smoke `FUZZ=rigor_{scriptpubkey,mint,supply,reorg}` | **PASS** (stdin one-shot, no crash) |

C++ encoder fact used in P5: `GetResMintScript(5)` is `OP_PUSHBYTES_1 0x05`, not `OP_5`. Coq was aligned to that frozen C++ rule. `OP_5` is the rejected second encoding.
