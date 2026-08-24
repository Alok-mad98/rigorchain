# RFC-017 RES Schedule — Formal Audit

| Field | Value |
|-------|-------|
| **RFC** | [RFC-017-SCHEDULED-RES-REWARD.md](../../rfc/RFC-017-SCHEDULED-RES-REWARD.md) |
| **Primary Coq module** | `formal_verification/coq/ResSchedule.v` |
| **Also touched** | `Selection.v` (E=246), `ConnectBlockMint.v`, `SupplyConservation.v`, `Properties.v` |
| **Audit date** | 2026-08-19 |
| **Method** | Extracted from compiled `.vo` via `Print Assumptions` and theorem inventory; numeric pins cross-checked against the Python spec |

This page records what the Coq development actually proves about RFC-017.
It does not claim runtime–Coq equivalence for the full node, and it does not
invent discharge of obligations that remain open elsewhere in Phase-4F/5.

---

## Theorem List

### ResSchedule.v (new)

| Name | Kind | Claim (compressed) |
|------|------|--------------------|
| `schedule_terminates` | Lemma | `k > 250 → W(k) = 0` |
| `schedule_sum_value` | Lemma | `Σ_{k=0..250} W(k) = 99450437` (atoms) |
| `max_issuance_within_cap` | Theorem | `RES_REWARD_EPOCH * schedule_sum ≤ RES_MAX_SUPPLY` |
| `cap_margin_value` | Lemma | Cap margin pins to `549563 * RES_REWARD_EPOCH` atoms |
| `reward_at_genesis` | Lemma | `W(0) = RES_REWARD_GENESIS = 40_000_000` |
| `reward_at_last_epoch_is_positive` | Lemma | `W(250) > 0` |
| `reward_after_last_epoch_is_zero` | Lemma | `W(251) = 0` |
| `reward_at_height_zero` | Lemma | `get_res_mint_reward 0 = W(0)` |
| `reward_decays_across_the_first_epoch` | Lemma | `get_res_mint_reward(210000) < get_res_mint_reward(0)` |
| `res_reward_for_epoch_nonneg` | Lemma | `∀ k, 0 ≤ W(k)` |
| `get_res_mint_reward_nonneg` | Lemma | `∀ h, 0 ≤ get_res_mint_reward h` |
| `res_reward_for_epoch_le_genesis` | Lemma | `∀ k, W(k) ≤ W(0)` |
| `reward_below_cap` | Lemma | `∀ h, get_res_mint_reward h ≤ RES_MAX_SUPPLY` |
| `sqrt_divisor_lower_bound` | Lemma | Fixed-point divisor ≥ 2^16 for every epoch |
| `get_res_mint_reward_zero_after_horizon` | Lemma | Height past epoch 251 ⇒ `W(h) = 0` |
| `res_cap_atom_boundaries` | Theorem | Exact cap admitted; one atom over rejected |
| `epoch_start_div` / `epoch_last_div` | Lemma | Height at epoch start / last block divides to epoch index |
| `get_res_mint_reward_epoch_start` / `_epoch_last` | Lemma | `W` on the first and last height of epoch `k` |
| `reward_crosses_first_epoch_boundary` | Lemma | `W(E-1)=W(0)`, `W(E)=W(1)`, and `W(E)<W(E-1)` |
| `reward_crosses_last_epoch_boundary` | Lemma | Last paying epoch still positive; first exhausted height is `W=0` |

### Selection.v (RFC-017 §5.3 pins)

| Name | Kind | Claim (compressed) |
|------|------|--------------------|
| `res_probability_target_not_vacuous` | Lemma | Active `T` selective at min weight |
| `mint_cost_is_1024_oru` | Lemma | `2^256 / T_unwindowed = 1024 * MIN_CONVERGENCE_AMOUNT` |
| `saturation_weight_is_32768_oru` | Lemma | Saturation weight pin at E=246 |

### ConnectBlockMint.v / SupplyConservation.v (schedule consumers)

| Name | Kind | Claim (compressed) |
|------|------|--------------------|
| `cb_res_mint_amount_nonneg` | Lemma | Scheduled mint amount ≥ 0 |
| `cb_res_mint_delta_le_genesis` | Lemma | Per-block RES delta ≤ `W(0)` |
| `cb_res_mint_delta_bounded` | Lemma | Delta is 0 or the scheduled amount under the cap check |
| `res_mint_respects_cap` | Theorem | Runtime cap check still holds (now redundant vs finite-sum) |
| `valid_res_cap` / `apply_block` reject | Definition + lemmas | Overshoot ⇒ `apply_block = None` (explicit connect reject) |
| `res_issuance_dual_insurance` | Theorem | Finite sum **and** runtime reject, with exact-cap / +1-atom pins |
| `winner_cap_overshoot_fail_closed` | Theorem | `W(h)>0` + winner + overshoot ⇒ no connecting block (include or omit mint) |
| `winner_positive_W_connect_iff` | Theorem | Winner + `W(h)>0`: connect iff header/chainwork/ORU, count=1, no overshoot |
| `winner_positive_W_connect_failure_iff` | Theorem | Under those structural conjuncts, fail iff wrong count or overshoot |
| `valid_block_connect_reject_classification` | Theorem | Connect false iff header ∨ chainwork ∨ ORU ∨ mismatch ∨ unauthorized ∨ without-winner ∨ cap |
| `mint_obligation_reject_paths` | Theorem | Obligation false iff mismatch ∨ unauthorized ∨ without-winner |
| `exhausted_schedule_connect_iff` / `no_winner_connect_iff` | Theorem | `W=0` or no winner: connect iff structural checks + count=0 + supply ≤ cap |
| `at_hard_cap_next_winner_mint_rejected` | Theorem | Supply already at cap + winner + `W>0` ⇒ connect false / `apply_block = None` |
| `last_paying_epoch_still_requires_mint` | Theorem | Last paying height still has `W>0` |
| `first_exhausted_epoch_winner_mint_rejected` | Theorem | First exhausted height + any mint ⇒ `apply_block = None` |
| `res_issuance_dual_insurance_connect_path` | Theorem | Dual insurance **plus** full connect-path classification, cap-next-mint, epoch boundaries, mint-shape amount |

---

## Proof Status

| Item | Status | Evidence |
|------|--------|----------|
| Cap identity as finite sum | **Closed** | `max_issuance_within_cap` by `vm_compute` |
| Schedule sum pin `99450437` | **Closed** | `schedule_sum_value`; matches C++ `static_assert` |
| Explicit cutoff `k > 250` | **Closed** | `schedule_terminates` |
| E=246 cost `C = 1024 ORU` | **Closed** | `mint_cost_is_1024_oru` |
| Height taken from block header (reorg-safe) | **Closed** in model | `cb_res_mint_amount` uses `hdr_height (blk_header b)` |
| `Print Assumptions` on all rows above | **Closed under the global context** | No axioms, no `Admitted` in these modules |
| C++ / Coq / Python `W(k)` pointwise | **Checked outside Coq** | 252 epochs `k = 0..251` compared by dump |
| Full-node Coq↔C++ equivalence | **Not claimed** | Phase-5 Draw/Supply equivalence remains partial |

Pinned numerics (atoms unless noted):

- `RES_COIN = 1_000_000` (6 decimals)
- `RES_REWARD_GENESIS = 40_000_000`
- `RES_MAX_SUPPLY = 21_000_000_000_000`
- `schedule_sum = 99_450_437`
- `RES_REWARD_EPOCH * schedule_sum = 20_884_591_770_000 ≤ RES_MAX_SUPPLY`
- `W(0)=40000000`, `W(1)=14142196`, `W(250)=10058`, `W(251)=0`

---

## Remaining Admits

**None** in `ResSchedule.v`, `Selection.v` (RFC-017 pins), `ConnectBlockMint.v`,
`ResMintObligation.v` (cap-overshoot reject + H-1 fail-closed + connect-path closed under abstract `cm_draw_won`), or
`SupplyConservation.res_mint_respects_cap`.

---

## Remaining Axioms

**None** for the theorems listed above (`Print Assumptions` → Closed under the global context).

---

## Counterexamples

None constructed. The pre-RFC regression witness retained in `Selection.v` is
`pre_a6_target_was_vacuous` (undivided `2^240`), which is **not** a counterexample to
E=246; it documents a fixed defect unrelated to the monetary exponent change.

---

## Soundness Assessments

1. **Cap as arithmetic fact plus runtime reject.** Because `W` terminates, maximum issuance under
   one-mint-per-block is a finite sum (`max_issuance_within_cap`). Independently,
   `apply_block` rejects any attempted mint that would exceed `RES_MAX_SUPPLY`
   (`res_issuance_dual_insurance`). Equal-to-cap is admitted; one atom over is not.
   Full `valid_block_connect` classification is `res_issuance_dual_insurance_connect_path`
   (**connect-path closed under abstract `cm_draw_won`**): winner + `W(h)>0` succeeds iff exactly one mint of
   `W(h)` under the cap; every known reject (overshoot, wrong count, unauthorized,
   without-winner, exhausted schedule, next mint at hard cap, wrong body amount as
   mint-shape) has a path. The winner flag is not owner/PCORB/Selection
   (`OpaqueBoundary.v`). Epoch first/last boundaries are lemmas. Not a C++ refinement.
   The delta clamp `res_mint_respects_cap` remains a restatement.

2. **Cost ratchet independence.** With constant `T`, mint cost `C` is a protocol
   constant. Floor price `C / W(k)` does not depend on ORU subsidy or demand.
   That claim is definitional in the model, not an inductive invariant over chain state.

3. **What this audit does *not* buy.** It does not prove that every C++ call site
   uses `GetResMintReward(height)` correctly beyond the sites already reviewed in
   RFC-017 §6, and it does not close Phase-5 cross-domain Draw equivalence.

4. **Golden-vector collateral.** Regenerating Inequality / SupplyScaffold vectors
   for E=246 exposed a frozen Phase-4F generator; the draw recomputation rule was
   corrected to the protocol σ-cross-product (see
   [DRAW_RECOMPUTATION_RFC017_NOTE.md](DRAW_RECOMPUTATION_RFC017_NOTE.md)).
   That repair is required for the regenerated vectors to mean what they say.
