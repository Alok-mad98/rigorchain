# Coq Load-Bearing Theorems (Coverage Audit)

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-COVERAGE-AUDIT-P0-001`  
**Date:** 2026-08-20  

This is the human-readable cone for PART 2. The full 2585-name list is `FORMAL_THEOREM_INVENTORY.json`.  
Unless noted, protocol-core theorems are expected **Closed under the global context** after a consistent rebuild (no `Axiom` / `Admitted` in sources).

Legend — **Method:** `vm` = `vm_compute`, `arith` = integer reasoning, `perm` = permutation/order, `struct` = state-transition structure, `neg` = explicit non-claim.

| Theorem / lemma | File | Purpose | Assumptions | Method | Depends on (sketch) |
|-----------------|------|---------|-------------|--------|---------------------|
| `max_issuance_within_cap` | ResSchedule.v | 210000 × schedule_sum ≤ RES_MAX_SUPPLY | None beyond Coq/Z | arith + pinned sum | `schedule_sum_value` |
| `schedule_sum_value` | ResSchedule.v | `schedule_sum = 99450437` | — | vm | reward table fold |
| `schedule_terminates` | ResSchedule.v | W(k)=0 for k>250 | — | struct | LAST_EPOCH |
| `reward_pin_table` | ResSchedule.v | Pins W(k) for audited epochs | — | vm | sqrt fixed-point |
| `reward_nonincreasing` | ResSchedule.v | W nonincreasing on schedule | — | vm check ⇒ thm | `reward_nonincreasing_check` |
| `cost_floor_nondecreasing_along_schedule` | ResSchedule.v | C/W rises as W falls | E=246 cost | arith | Selection mint cost |
| `res_exact_cap_admitted` | ResSchedule.v | supply+amount = cap ⇒ accept | — | arith | leb |
| `res_overshoot_one_atom_rejected` | ResSchedule.v | +1 atom over ⇒ reject | — | arith | leb |
| `convergence_root_order_independent` | OrderIndependence.v | Permutation of F ⇒ same C | Distinct sort keys as stated | perm | ConvergeRoot/SHA |
| `select_winner_order_independent` | OrderIndependence.v | Same set ⇒ same winner | Positive weights, distinct txids | perm | Selection |
| `mint_cost_is_1024_oru` | Selection.v | E=246 ⇒ C=1024 | Constant lock | arith | RES_PROBABILITY_TARGET |
| `windowing_preserves_non_vacuity` | Selection.v | T/W still selective | — | arith | window=32 |
| `txid_wins_at_most_once` (family) | WindowExclusion.v | A-9-C unpaid filter | Model window | struct | Selection |
| `draw_pipeline_e2e` (family) | DrawCandidateRoot.v | Eligible list = DCR = winner path | — | struct | Selection/Window |
| `dual_pool_conservation` | DualPoolConservation.v | Ordinary RES in≥out | — | arith | sum_asset |
| `apply_block_res_net_bounded_by_scheduled_mints` | DualPoolConservation.v | Block RES net ≤ scheduled mint | apply_block success | struct | mint shape |
| `winner_must_mint` | ResMintObligation.v | H-1 under `cm_draw_won` | **Abstract flag** | struct | ConnectBlockMint |
| `cap_overshoot_rejects_apply_block` | ResMintObligation.v | Cap fail-closed | — | struct | valid_res_cap |
| `oru_supply_bounded_by_block_count` | SupplyConservation.v | ORU ≤ f(#blocks) | Model subsidy | arith | ConnectBlockMint |
| `supply_deterministic` | SupplyConservation.v | Replay path unique supply | — | struct | apply_block |
| `rollback_restores_previous_state` | ReplayInvariants.v | Rollback = prior snapshot | — | struct | history stack |
| `apply_block_succeeds_implies_script_shape` | StateTransition.v | Accepted block ⇒ shape OK | — | struct | ScriptShape |
| `we_do_not_claim_cm_draw_won_is_owner_pcorb` | OpaqueBoundary.v | Documents non-claim | — | neg | — |
| `we_do_not_claim_script_derives_asset` | OpaqueBoundary.v | Documents non-claim | — | neg | — |
| `we_do_not_claim_rs_res_supply_equals_utxo_res` | OpaqueBoundary.v | Documents non-claim | — | neg | — |
| `golden_*` (215) | Kernel/*Harness.v | Finite L1↔L2 bytes/bools | Concrete vectors | vm | Kernel + monetary preds |
| Phase-5 `DCS_Impossible` catalogs | Phase5Equivalence/* | Honest non-closure of refinement | Catalog status | struct/status | Phase4F bridges |

### Dependency chain (monetary, simplified)

```
RigorSha256 → RigorHash → CanonicalHashKernel
                ↓
        Sigma / ConvergeRoot / DrawCandidateRoot
                ↓
        Selection (E=246, W=32) → WindowExclusion
                ↓
        ResSchedule (W(k), cap)
                ↓
        ConnectBlockMint → ResMintObligation → DualPoolConservation
                ↓
        StateTransition.apply_block / SupplyConservation
                ↓
        Kernel harness (NEG / XFAM / …)
```

Phase-4F/5 modules hang off correspondence catalogs and do **not** strengthen the monetary cone; many rows record impossibility of L2=L3 identity.
