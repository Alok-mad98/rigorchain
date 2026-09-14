# Coq Load-Bearing Theorems (Coverage Audit)

**Document status:** Security audit  
**Date:** 2026-09-14  

Live issuance is RFC-020. Issuance theorems live in `ResSchedule.v` and
`Supply/SupplySchedule.v`. Names match `tools/res_continuous_price_schedule.py`.
Public kernel status is `audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`:

```
MATURITY_LEVEL=4.9
REFINEMENT_PROVED=false
equivalence_claimed = false
```

ALLOWED: Coq consensus kernel model has formally verified invariants.
NOT ALLOWED: production C++ node verified; ConnectBlock refinement proved.

The machine inventory is `FORMAL_THEOREM_INVENTORY.json` (regenerate with
`formal_verification/tools/gen_theorem_inventory.py`). Phase-4F/4G/5 catalog
modules are paperwork; do not add them to a “proved” count.

Legend — **Method:** `vm` = `vm_compute`, `arith` = integer reasoning,
`perm` = permutation/order, `struct` = state-transition structure,
`neg` = explicit non-claim.

| Theorem / lemma | File | Purpose | Method |
|-----------------|------|---------|--------|
| `predicate_shift_is_exact` | ResSchedule.v | `RES_DESTROY_SHIFT = 50 − 10` | vm |
| `genesis_destroy_cost_is_exact` | ResSchedule.v | `destroy_cost(0) = 1024 ORU`; min burn 1024 atoms | vm |
| `destroy_cost_strictly_increasing` | ResSchedule.v | `destroy_cost(h)` nondecreasing; `destroy_cost(2²⁰) > destroy_cost(0)` | arith + vm |
| `no_free_mint_anywhere` | ResSchedule.v | `R=1, B=0` rejected at every `h ≥ 0` | arith |
| `predicate_equals_destroy_cost` | ResSchedule.v | pole inverse matches display destroy cost at genesis | vm |
| `predicate_matches_destroy_cost` | ResSchedule.v | same at heights 0 and 1, ±1 atom | vm |
| `cap_is_closed_form` | ResSchedule.v | `(128/1024)·2²⁵ · RES_COIN = 2²² RES` | vm |
| `cap_is_upper_bound` | ResSchedule.v | `cumulative_res_at h ≤ RES_MAX_SUPPLY` | arith |
| `no_overflow_in_uint128` | ResSchedule.v | mint products fit `2¹²⁸−1` | vm |
| `res_cap_atom_boundaries` | ResSchedule.v | exact cap admitted; +1 atom rejected | arith |
| `res_schedule_finite` | SupplySchedule.v | `destroy_cost(h) ≥ 0` | arith |
| `res_height_monotonic` | SupplySchedule.v | authorized heights: destroy cost nondecreasing | arith |
| `res_derived_cap_identity` | SupplySchedule.v | cap is `2²²` RES | vm |
| `oru_subsidy_constant` | SupplySchedule.v | `R(0)=0`; `h≥1` pays 128 ORU | struct |
| `res_issuance_soundness` | RESIssuance.v | accepted mint satisfies the pole inequality (Coq model only) | struct |
| `res_mint_requires_body` | RESIssuance.v | coinbase cannot mint RES (Coq model only) | struct |
| `golden_*` (215) | Kernel/*Harness.v | finite L1↔L2 bytes/bools | vm |

### Dependency chain (issuance)

```
ResSchedule / SupplySchedule
    destroy_cost(h), pole predicate, cap 2²² RES, flat 128 ORU
                ↓
        body-tx mint predicate (model)
                ↓
        Kernel harness (NEG / golden)
```

Archive lottery models are not current mint evidence.
