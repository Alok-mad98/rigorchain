> Alias deliverable name for `RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md`.

# RES Monetary Formal Verification — Gap Analysis (RFC-017)

**Task:** `TASK-RIGOR-RES-MONETARY-POLICY-FINAL-VERIFICATION-P0-001` §5  
**Date:** 2026-08-20  
**Module focus:** `formal_verification/coq/ResSchedule.v` (+ consumers)  
**`equivalence_claimed`:** `false`

Classification: **DONE** | **PARTIAL** | **MISSING**

---

## Checklist

| ID | Property | Status | Evidence / gap |
|----|----------|--------|----------------|
| A | Reward monotonic decrease | **DONE** | `reward_nonincreasing` (all adjacent pairs `k < 250` via `forallb` + `vm_compute`) |
| B | Cost floor monotonic increase | **DONE** | `cost_floor_nondecreasing_along_schedule` given constant `C > 0` and `W(k+1) > 0` |
| C | Explicit termination | **DONE** | `schedule_terminates`, `reward_after_last_epoch_is_zero` |
| D | No issuance after cutoff | **DONE** (model) | `get_res_mint_reward_zero_after_horizon`; C++ reject paths audited separately |
| E | Supply cap independent of demand | **DONE** | `max_issuance_within_cap` — finite sum, no demand hypothesis |
| F | Replay determinism (reward) | **PARTIAL** | Header-height scheduling proved in ConnectBlockMint / supply lemmas; full-node C++ refinement **not** claimed |
| G | Differential pins `k∈{0,1,10,100,250,251}` | **DONE** | `reward_pin_table` + Python/C++ parity test |
| H | Schedule sum pin `99450437` | **DONE** | `schedule_sum_value` |
| I | Cap atom boundaries | **DONE** | `res_cap_atom_boundaries` |
| J | Epoch boundary constancy | **DONE** | `get_res_mint_reward_epoch_start` / `_last`, cross-boundary lemmas |
| K | C++ ↔ Coq refinement of ConnectBlock | **MISSING** | Honest boundary; keep `equivalence_claimed = false` |
| L | Maturity / Owner / PCORB as Coq theorems | **MISSING** | Outside RFC-017 schedule math; see coverage gap eval |
| M | Post-schedule winner-must-mint liveness | **MISSING** (policy) | Security finding, not a schedule-math hole |

---

## Print Assumptions (2026-08-20)

Closed under the global context:

- `max_issuance_within_cap`
- `reward_nonincreasing`
- `cost_floor_nondecreasing_along_schedule`
- `reward_pin_table`

No new axioms introduced for these properties.

---

## What “DONE” does *not* mean

- It does **not** mean the shipping `bitcoind` binary is a Coq extract.
- It does **not** authorize marketing “mathematically proven complete monetary correctness.”
- Allowed phrase: **formally verified monetary invariants** (model + differential gate).

---

## Recommended next work (out of scope for this task)

1. Optional: Coq-enforce additional NegativeCore rows for mid-schedule wrong `W` (k=10/100/250) if harness cost is acceptable.  
2. Do not flip `equivalence_claimed` without a dedicated refinement program.
