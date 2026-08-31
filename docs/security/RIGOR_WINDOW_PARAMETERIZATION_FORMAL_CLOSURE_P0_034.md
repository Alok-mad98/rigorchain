# RIGOR Window Parameterization Formal Closure

**Task:** TASK-RIGOR-WINDOW-PARAMETERIZATION-FORMAL-CLOSURE-P0-034  
**Priority:** P0 — Formal Verification / Parameterization Closure  
**Mode:** Formal parameterization. Production `W=32`, `T`, `F`, maturity, subsidy, sighash, ScriptPubKey, and Ed25519 were **not** modified. W=36 is a Coq/C++-test instance only.  
**Date:** 2026-08-29  
**Companions:** [P0-032](./RIGOR_W32_REORG_STABILITY_AUDIT_P0_032.md), [P0-033](./RIGOR_W36_SIX_HOUR_WINDOW_FEASIBILITY_AUDIT_P0_033.md)

```
CURRENT_W                  = 32

GENERIC_WINDOW_MODEL       = PASS
GENERIC_REORG_PROOF        = PASS
GENERIC_CACHE_PROOF        = PASS
GENERIC_MATURITY_PROOF     = PASS
GENERIC_SUPPLY_PROOF       = PASS
T(W) PARAMETERIZATION      = PASS

W32 SPECIALIZATION         = PASS
W36 SPECIALIZATION         = PASS

POWER_OF_TWO_DEPENDENCY    = NONE   (generic Coq model)
C++ PARAMETERIZATION GAP   = FOUND  (shipping >> 5 / static_assert W==1<<5)

COQ_ADMITTED               = 0
C++ REFINEMENT              = NOT PROVED

PRODUCTION_W               = 32
PRODUCTION_W36_LANDED      = NO
```

Honesty lock:

```
Window model:                 W-generic
Current production instance:  W=32
Candidate instance:           W=36
Production W=36:              NOT LANDED
refinement_proved:            false
equivalence_claimed:          false
```

**Final verdict: `PARAMETERIZATION_PARTIAL`**

The W-generic Coq model is complete and compiles with zero `Admitted`/`Axiom`. W=32 and W=36 are both legal instances. The generic theorems do **not** assume `W = 32` or `W = 2^k`. Production C++ still encodes the shipping instance as a right shift (`UNWINDOWED >> 5`) with `static_assert(W == 1<<5)`. That encoding is classified as **IMPLEMENTATION_OPTIMIZATION**, not protocol mathematics. It is a documented C++ parameterization gap, not a blocker of the generic model. C++ refinement remains unproved.

**W=32 is an instantiated protocol parameter, not a mathematical requirement.**  
**W=36 remains a candidate protocol amendment and has not been landed in production.**  
**W=36 specialization = PASS does not mean W36 production = safe/ready.**

---

## 1. What changed / what did not

| Item | This task |
|------|-----------|
| `CONVERGENCE_DRAW_WINDOW` production | **unchanged (32)** |
| `RES_PROBABILITY_TARGET` hex | **unchanged** |
| F / draw production code | **unchanged** |
| Coinbase / RES maturity | **unchanged (100)** |
| Subsidy, ASERT, sighash, ScriptPubKey, Ed25519 | **unchanged** |
| `formal_verification/rigor_v01/WindowGeneric.v` | **added** |
| `WindowTarget.v` / `WindowSpecialization.v` | **added** |
| `W32ReorgStability.v` | **rewritten as W=32 instance of the generic model** |
| `core/src/test/window_parameterization_tests.cpp` | **added** (does not change production W) |

Architecture:

```
W32ReorgStability (P0-032 names)
        ↓
WindowGeneric  +  WindowTarget     (W ∈ ℤ, W > 1)
        ↓
WindowSpecialization
   W=32 shipping instance
   W=36 formal candidate   ← NOT production
```

There is no `W36ReorgStability.v` that copies 32→36. W=36 theorems are instances of the same generic lemmas.

---

## 2. Generic window definition

```
W_ok W  :=  1 < W

window_of_source(s, W)  =  [max(0, s − W + 1), s]
draw_window(h, W)       =  [h − W, h − 1]
clamped_draw(h, W)      =  [max(0, h − W), h − 1]
                        =  window_of_source(h − 1, W)
```

Proved in `WindowGeneric.v`:

| Theorem | Statement |
|---------|-----------|
| `window_length_full` | `W_ok W → W−1 ≤ s → length = W` |
| `window_length_genesis_truncation` | `0 ≤ s < W−1 → window = [0, s]`, length `s+1` |
| `window_start_nonneg` | start ≥ 0 (no negative heights) |
| `in_window_height_nonneg` | `s ≥ 0` and membership ⇒ `h ≥ 0` |
| `draw_length_is_W` | unclamped draw length = W |
| `clamped_draw_agrees_with_parent_source` | clamped draw of `h` = source window of `h−1` |
| `not_floor_partition` | `start(2W, W) ≠ W · ((2W)/W)` |
| `coincides_with_floor_slice_only_at_slice_end` | coincidence only at `s = W·g + (W−1)` |
| `windows_overlap` | one height belongs to many source windows |
| `W_ok_holds_for_non_pow2` | 3, 36, 48 are legal |
| `thirty_six_is_not_a_power_of_two` | ∀k ≥ 0, 36 ≠ 2^k |

Power-of-two is **not** a hypothesis of any generic theorem.

---

## 3. T(W) parameterization

```
UNWINDOWED = floor(2^246 / MIN_CONVERGENCE_AMOUNT)
T(W)       = floor(UNWINDOWED / W)
```

Integer division, not `>> 5`. The shipping shift is proved equivalent **only** at W=32:

```
T(32) = UNWINDOWED / 32 = UNWINDOWED >> 5     (T32_matches_shipping_shift_encoding)
T(36) ≠ UNWINDOWED >> 5                       (shift5_is_not_T_of_36)
```

A-9C pair uniqueness (`T_floor_bracket_unique`):

```
0 < W  →  (T·W ≤ UNWINDOWED < (T+1)·W  ↔  T = T(W))
```

Neutrality (`expected_wins_neutral_integer`):

```
T(W)·W·n  =  (UNWINDOWED − UNWINDOWED mod W)·n
```

So when T is retuned with W, the product T·W stays within remainder `< W` of the **same** UNWINDOWED. Expected wins scale as `n/C`, not as `W · n/C`. Mint cost `C = 1024 ORU` is independent of W (`mint_cost_independent_of_W`).

**Pinned T with a different window is a different protocol:**

```
T_pinned_old_W_is_not_generic :
  T(32) ≠ T(36)
  ∧  ¬ (T(32)·36 ≤ UNWINDOWED < (T(32)+1)·36)

stale_T32_with_W36_overshoots :
  UNWINDOWED < T(32)·36
```

W changed + T unchanged is **not** equivalent to the A-9C pair. Formalized; production T not rewritten.

Golden residues (formal only): `UNWINDOWED mod 32 = 27`, `mod 36 = 31`. Saturation `C·W`: 32768 ORU at 32, 36864 ORU at 36.

---

## 4. Reorg (generic)

| Theorem | Content |
|---------|---------|
| `reorg_preserves_window_boundary` | `branch_window a s W = branch_window b s W` |
| `reorg_changes_contents_only` | interval equal; `contents_at` differs across branches |
| `deep_reorg_depths_relative_to_W` | depths `W−1`, `W`, `W+1`, `2W` do not move the interval |
| `same_height_distinct_branches_distinct_hash` | height is not identity |
| `orphan_state_not_canonical` | `OrphanOwner` is not canonical |

P0-032 names in `W32ReorgStability.v` are corollaries at `W := 32`.

---

## 5. Cache

```
CacheByBlockHash     → reorg-safe = true
CacheByHeight        → false
CacheByGroupId       → false
CacheByFloorSlice    → false
```

`reorg_changes_cache_key` / `old_hash_does_not_alias_new_hash`: distinct branches at the same height have distinct hashes, so a hash-keyed store cannot serve old canonical F under the new hash.

This is a **model** of the live C++ maps (`g_window_contribution_cache`, `g_draw_set_cache`, `g_witness_cache`), which are keyed by `uint256` block hash. **Not a refinement of `validation.cpp`.**

---

## 6. Maturity

```
COINBASE_MATURITY = 100
coinbase_mature spend coin  :=  (spend − coin ≥? 100)
```

The predicate does not take W.

| Theorem | Content |
|---------|---------|
| `maturity_absolute_depth` | `+100` matures, for every W |
| `window_width_does_not_mature` | `W_ok W → W < 100 → +W` does not mature |
| `maturity_not_derived_from_window` | membership ⇏ maturity |
| `maturity_not_div_W_rule` | always `≥ 100`, never `≥ W` / `100/W` / `N·W` as the rule |
| `W36_maturity_same_as_W32` | same predicate |

---

## 7. Supply / consensus independence

W-**dependent** (expected):

- F membership interval
- T(W)
- C = Hash256(sorted F txids) *values* (via F)
- per-draw saturation `C·W`

W-**independent** (generic model + C++ scan):

| Module | Status |
|--------|--------|
| ORU subsidy | not a function of draw W |
| Coinbase / RES mint maturity | 100 |
| RES cap / RFC-017 `W(k)` | different symbol; `<< 32` in `ResIntegerSqrt` is fixed-point, not draw W |
| ASERT | τ = 43200 s; not W |
| sighash / Ed25519 | not W |
| ScriptPubKey / asset classifier | `32` = pubkey width, not W |
| serialization layout | no W field |

`oru_subsidy_model` in Coq is a W-free stub documenting independence. Full RFC-017 schedule remains in `ResSchedule.v` / lock header and was not edited.

---

## 8. Specializations and golden vectors

### W=32 (shipping instance)

| s | start | end | length |
|---|-------|-----|--------|
| 0 | 0 | 0 | 1 |
| 31 (W−1) | 0 | 31 | 32 |
| 32 (W) | 1 | 32 | 32 |
| 33 (W+1) | 2 | 33 | 32 |
| 64 (2W) | 33 | 64 | 32 |
| 1023 | 992 | 1023 | 32 |

P0-032 scenarios A/B recovered (`W32_task_scenario_*`).

### W=36 (formal candidate only)

| s | start | end | length |
|---|-------|-----|--------|
| 0 | 0 | 0 | 1 |
| 35 (W−1) | 0 | 35 | 36 |
| 36 (W) | 1 | 36 | 36 |
| 37 (W+1) | 2 | 37 | 36 |
| 72 (2W) | 37 | 72 | 36 |

`PRODUCTION_W36_LANDED = false`. `production_W_is_32`.

---

## 9. C++对照 (no refinement)

Production formula at `convergence_logic.cpp` (`window_first = max(0, connect_height − W + 1)`) matches `window_of_source`. Tests assert the generic formula **and** that production `W` is still 32.

| Hit | Class |
|-----|--------|
| `CONVERGENCE_DRAW_WINDOW = 32` | **PROTOCOL** (shipping instance) |
| `CONVERGENCE_DRAW_WINDOW_LOG2 = 5` | **IMPLEMENTATION_OPTIMIZATION** |
| `static_assert(W == 1<<5)` | **IMPLEMENTATION_OPTIMIZATION** |
| `T := UNWINDOWED >> 5` (prose + tests) | **IMPLEMENTATION_OPTIMIZATION** (equals `/32`) |
| `RES_PROBABILITY_TARGET_CONST` hex | **PROTOCOL** (shipping T(32)) |
| `MIN_BLOCKS_TO_KEEP = 320` | operational `288+W`; not F |
| LRU `4·W` | capacity, hash-keyed |
| `window_is_a_power_of_two` in `Selection.v` | **PARAMETERIZATION_GAP** (legacy shipping pin; not modified) |
| witness v1 / 32-byte pk | **UNRELATED_CONSTANT** |
| RFC-017 `<< 32` isqrt | **UNRELATED_CONSTANT** |
| `nBits 0x207fffff` | **UNRELATED_CONSTANT** |

No mechanical replacement of `32` was performed.

**PARAMETERIZATION_GAP list:**

1. C++ `>> 5` / `LOG2` / power-of-two `static_assert` (shipping encoding).  
2. `formal_verification/coq/legacy_account_model/Selection.v` still pins `W := 32` and `window_is_a_power_of_two` (shipping spec mirror; left unchanged so production T is not formally retuned).  
3. C++ ↛ Coq refinement of `validation.cpp` (`refinement_proved = false`).

---

## 10. Tests

New: `core/src/test/window_parameterization_tests.cpp`

Covers W ∈ {2, 3, 4, 8, 16, 32, **36**, 48, 64}. W=36 is the non-power-of-two focus. Asserts production W remains 32 and production T hex equals `T(32)`, not `T(36)`.

Executed (2026-08-29):

```
./src/test/test_rigor --run_test=window_parameterization_tests,w32_reorg_stability_*
  + runtime_supply_* + convergence_window_ui_tests
→ 50 + 54 cases, 0 errors

./src/test/test_rigor   (full unit suite)
→ 981 cases, 0 errors (1 pre-existing Boost warning)
```

Production `CONVERGENCE_DRAW_WINDOW` remains 32. `make -j2 test/test_rigor` linked the new object. Full `make check` recursive (functional Python suite) was not required to validate this formal parameterization; the unit binary is the consensus-adjacent check.

Existing `w32_reorg_stability_tests.cpp` was not logically modified.

---

## 11. Coq build

```
coqc -R formal_verification/rigor_v01 RigorV01 \
  WindowGeneric.v WindowTarget.v WindowSpecialization.v W32ReorgStability.v
```

Result: **PASS**. `Admitted` = 0. `Axiom` = 0.

Listed in `_CoqProject` and `_CoqProject.protocol-core`.

---

## 12. Verdict

```
PARAMETERIZATION_PARTIAL
```

Generic formalization **succeeded**. W=32/W=36 are legal instances. There is **no** hidden W=32 hypothesis in `WindowGeneric.v` / `WindowTarget.v`. Remaining gaps are C++ shipping encoding, the legacy `Selection.v` pin, and unproved refinement — not a failure to parameterize the window mathematics.

Do not read this as authorization to land W=36. That remains a Level-5 `(W, T)` freeze amendment (P0-033).
