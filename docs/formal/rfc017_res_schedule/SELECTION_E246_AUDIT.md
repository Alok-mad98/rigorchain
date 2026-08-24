# RFC-017 Selection E=246 — Formal Audit

| Field | Value |
|-------|-------|
| **RFC** | RFC-017 §5.3 |
| **Module** | `formal_verification/coq/Selection.v` |
| **Change** | `RES_PROBABILITY_TARGET_UNWINDOWED`: `2^240 / MIN` → `2^246 / MIN` |
| **Audit date** | 2026-08-19 |

---

## Theorem List

| Name | Role after RFC-017 |
|------|--------------------|
| `res_probability_target_not_vacuous` | Re-evaluated at E=246; still holds |
| `res_probability_target_rejects_max_digest` | Still filters max digest at min weight |
| `mint_cost_is_1024_oru` | **New pin:** `C = 2^(256-E) = 1024` ORU per mint |
| `saturation_weight_is_32768_oru` | **New pin:** weight where the filter saturates |
| `pre_a6_target_was_vacuous` | Unchanged regression witness (undivided 2^240) |
| `window_is_a_power_of_two` / `target_is_unwindowed_divided_by_window` | Untouched (A-9-C) |

---

## Proof Status

| Claim | Status |
|-------|--------|
| Target non-vacuous at E=246 | Closed (`vm_compute`) |
| Mint cost exactly 1024 ORU | Closed (`mint_cost_is_1024_oru`) |
| Vacuity margin | Boundary at E=261; E=246 retains 2^15 headroom (documented in module comments) |
| Axioms / Admits | None (`Print Assumptions` closed) |

---

## Remaining Admits

None.

---

## Remaining Axioms

None.

---

## Counterexamples

None for E=246. The A-6 vacuity counterexample applies only to the pre-A-6 undivided target.

---

## Soundness Assessments

Moving E from 240 to 246 is a **monetary** decision, not a correctness fix.
The formal layer’s job is to make silent repricing impossible: changing the
exponent without updating `mint_cost_is_1024_oru` / saturation pins fails the
build. Cap utilisation under 100% burn is an economic consequence recorded in
RFC-017 §5.4, not a Coq obligation.
