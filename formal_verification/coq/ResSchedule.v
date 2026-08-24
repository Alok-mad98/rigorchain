(** ResSchedule.v — the RFC-017 scheduled RES mint reward.

    Mirrors [Consensus::ResRewardForEpoch] / [Consensus::GetResMintReward] in
    `core/src/consensus/rigor_consensus_lock.h`.

    The draw target T is a constant (see Selection.v), so the ORU cost of one mint is the
    protocol constant C = 2^(256-E). Letting the reward decay on a height schedule makes the cost
    floor per RES equal C / W(k): a constant divided by a protocol function, with no dependence on
    demand, on the ORU subsidy, or on any chain state.

    WHAT THIS FILE BUYS. Under a fixed reward, "cumulative RES stays under the cap" is an
    inductive invariant maintained by a runtime check — the model has to carry it through every
    block. Because W terminates, the same statement becomes a FINITE SUM of 251 integers and
    therefore a closed arithmetic fact ([max_issuance_within_cap]), independent of chain length,
    of adversarial behaviour, and of the runtime check. Dual insurance (final form,
    TASK-FV-MODEL-CAP-BOUNDARY-AND-H1-001): (1) this finite-sum identity; (2)
    [apply_block] rejects cap overshoot ([valid_res_cap] / [bad-rigor-res-max-supply]);
    (3) [res_mint_respects_cap] remains a redundant delta-clamp restatement.
    Equal-to-cap is admitted; one atom over is rejected. No retargeting.
    Not a C++ refinement. *)

Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.

Local Open Scope Z_scope.

(** ** Constants *)

(** Atoms per 1.000000 RES. Six decimal places (RFC-017 §7.2). *)
Definition RES_COIN : Z := 1000000.

(** Hard cap in atoms. The 21M figure is unchanged; only the scale moved. *)
Definition RES_MAX_SUPPLY : Z := 21000000 * RES_COIN.

(** W(0) = 40.000000 RES. *)
Definition RES_REWARD_GENESIS : Z := 40 * RES_COIN.

(** Reward epoch length in blocks; shares the ORU halving cadence. *)
Definition RES_REWARD_EPOCH : Z := 210000.

(** Last epoch paying a non-zero reward. *)
Definition RES_REWARD_LAST_EPOCH : nat := 250.

(** ** The schedule

    W(k) = floor(RES_REWARD_GENESIS * (k+1)^(-3/2)), evaluated in fixed point at 2^-16.

    [Z.sqrt] is floor(sqrt(.)) on non-negative arguments and [Z.div] is floor division there, so
    this is literally the C++ expression: [isqrt(x << 32) = floor(sqrt x * 2^16)], and the two
    shifts cancel through the division.

    The [RES_REWARD_LAST_EPOCH] guard is a RULE, not an emergent property. Without it the
    schedule would end wherever the reward happens to floor to zero, which depends on
    [RES_COIN] — the horizon would be a consequence of a display choice. With the cutoff stated,
    the admissible genesis reward expressed in RES is independent of the scale. *)
Definition res_reward_for_epoch (k : nat) : Z :=
  if Nat.ltb RES_REWARD_LAST_EPOCH k then 0
  else
    let e := Z.of_nat k + 1 in
    Z.shiftl RES_REWARD_GENESIS 16 / Z.sqrt (Z.shiftl (e * e * e) 32).

(** Reward for a block at the given height. Zero once the schedule is exhausted. *)
Definition get_res_mint_reward (height : Z) : Z :=
  if Z.ltb height 0 then 0
  else res_reward_for_epoch (Z.to_nat (height / RES_REWARD_EPOCH)).

(** ** Termination *)

Lemma schedule_terminates :
  forall k : nat, (RES_REWARD_LAST_EPOCH < k)%nat -> res_reward_for_epoch k = 0.
Proof.
  intros k Hk. unfold res_reward_for_epoch.
  replace (Nat.ltb RES_REWARD_LAST_EPOCH k) with true
    by (symmetry; apply Nat.ltb_lt; exact Hk).
  reflexivity.
Qed.

(** ** The cap identity

    At most one mint may occur per block and each epoch is exactly [RES_REWARD_EPOCH] blocks, so
    maximum conceivable issuance is [RES_REWARD_EPOCH * schedule_sum] — with no assumption
    whatsoever about demand, burn rate, or participant behaviour.

    Because the schedule terminates, the sum is finite and the whole statement is decidable by
    computation. *)

Definition schedule_sum : Z :=
  fold_left Z.add (map res_reward_for_epoch (seq 0 (S RES_REWARD_LAST_EPOCH))) 0.

(** Pins the computed value so any drift in W0, in the exponent, or in the cutoff is a proof
    failure rather than a silent change to the monetary schedule. Matches the C++
    [static_assert(ResScheduleSum() == 99450437)]. *)
Lemma schedule_sum_value : schedule_sum = 99450437.
Proof. vm_compute. reflexivity. Qed.

Theorem max_issuance_within_cap :
  RES_REWARD_EPOCH * schedule_sum <= RES_MAX_SUPPLY.
Proof. vm_compute. discriminate. Qed.

(** The margin is 0.55% of the cap: the schedule uses 99.45% of what it is allowed. *)
Lemma cap_margin_value :
  RES_MAX_SUPPLY - RES_REWARD_EPOCH * schedule_sum = 549563 * RES_REWARD_EPOCH.
Proof. vm_compute. reflexivity. Qed.

(** ** Sanity pins on the endpoints *)

Lemma reward_at_genesis : res_reward_for_epoch 0 = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma reward_at_last_epoch_is_positive :
  0 < res_reward_for_epoch RES_REWARD_LAST_EPOCH.
Proof. vm_compute. reflexivity. Qed.

Lemma reward_after_last_epoch_is_zero :
  res_reward_for_epoch (S RES_REWARD_LAST_EPOCH) = 0.
Proof. apply schedule_terminates. auto. Qed.

(** Height 0 sits in epoch 0, and the reward has strictly decayed one epoch later. *)
Lemma reward_at_height_zero : get_res_mint_reward 0 = RES_REWARD_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma reward_decays_across_the_first_epoch :
  get_res_mint_reward RES_REWARD_EPOCH < get_res_mint_reward 0.
Proof. vm_compute. reflexivity. Qed.

(** ** Bounds

    The supply layer needs two facts about an arbitrary height: a mint delta never subtracts from
    cumulative RES, and one mint alone cannot breach the cap. Both reduce to a lower bound on the
    divisor, since the numerator is a positive constant. *)

Require Import Coq.micromega.Lia.

(** The fixed-point divisor is at least 2^16 for every epoch, because (k+1)^3 >= 1. This is what
    makes the reward well defined and bounded above by W(0). *)
Lemma sqrt_divisor_lower_bound :
  forall k : nat,
    65536 <= Z.sqrt (Z.shiftl ((Z.of_nat k + 1) * (Z.of_nat k + 1) * (Z.of_nat k + 1)) 32).
Proof.
  intros k. set (e := Z.of_nat k + 1).
  assert (He : 1 <= e) by (unfold e; pose proof (Nat2Z.is_nonneg k); lia).
  assert (Hc : 1 <= e * e * e) by nia.
  assert (Hcube : 2 ^ 32 <= Z.shiftl (e * e * e) 32).
  { rewrite Z.shiftl_mul_pow2 by lia.
    replace (2 ^ 32) with 4294967296 by (vm_compute; reflexivity).
    nia. }
  replace 65536 with (Z.sqrt (2 ^ 32)) by (vm_compute; reflexivity).
  apply Z.sqrt_le_mono. exact Hcube.
Qed.

Lemma res_reward_for_epoch_nonneg : forall k : nat, 0 <= res_reward_for_epoch k.
Proof.
  intros k. unfold res_reward_for_epoch.
  destruct (Nat.ltb RES_REWARD_LAST_EPOCH k); [ apply Z.le_refl | ].
  cbv zeta. pose proof (sqrt_divisor_lower_bound k) as Hlb.
  apply Z.div_pos; [ vm_compute; discriminate | lia ].
Qed.

Lemma get_res_mint_reward_nonneg : forall h : Z, 0 <= get_res_mint_reward h.
Proof.
  intros h. unfold get_res_mint_reward.
  destruct (Z.ltb h 0); [ apply Z.le_refl | apply res_reward_for_epoch_nonneg ].
Qed.

(** The reward never exceeds W(0): the divisor only grows with k. *)
Lemma res_reward_for_epoch_le_genesis :
  forall k : nat, res_reward_for_epoch k <= RES_REWARD_GENESIS.
Proof.
  intros k. unfold res_reward_for_epoch.
  destruct (Nat.ltb RES_REWARD_LAST_EPOCH k); [ vm_compute; discriminate | ].
  cbv zeta. pose proof (sqrt_divisor_lower_bound k) as Hlb.
  apply Z.le_trans with (m := Z.shiftl RES_REWARD_GENESIS 16 / 65536).
  - apply Z.div_le_compat_l; [ vm_compute; discriminate | lia ].
  - vm_compute; discriminate.
Qed.

(** A single mint can never on its own exceed the cap. *)
Lemma reward_below_cap : forall h : Z, get_res_mint_reward h <= RES_MAX_SUPPLY.
Proof.
  intros h. unfold get_res_mint_reward.
  destruct (Z.ltb h 0); [ vm_compute; discriminate | ].
  apply Z.le_trans with (m := RES_REWARD_GENESIS).
  - apply res_reward_for_epoch_le_genesis.
  - vm_compute; discriminate.
Qed.

(** After the last paying epoch the schedule is identically zero: a draw may
    still name a winner (T is constant) but it does not authorize a mint. *)
Lemma get_res_mint_reward_zero_after_horizon :
  forall h : Z,
    Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH <= h ->
    get_res_mint_reward h = 0.
Proof.
  intros h Hh.
  unfold get_res_mint_reward.
  assert (Hnn : 0 <= h).
  { unfold RES_REWARD_EPOCH, RES_REWARD_LAST_EPOCH in Hh. lia. }
  replace (Z.ltb h 0) with false
    by (symmetry; apply Z.ltb_ge; exact Hnn).
  apply schedule_terminates.
  apply Nat2Z.inj_lt.
  rewrite Z2Nat.id
    by (apply Z.div_pos; [exact Hnn | unfold RES_REWARD_EPOCH; lia]).
  apply Z.le_succ_l.
  apply Z.div_le_lower_bound; [unfold RES_REWARD_EPOCH; lia | exact Hh].
Qed.

(** ** Runtime cap-fit (dual insurance with [max_issuance_within_cap])

    The finite-sum identity proves no one-mint-per-block walk of [W] can
    exhaust the cap. The comparison below is the independent runtime gate:
    [supply + amount <= RES_MAX_SUPPLY]. Equal-to-cap is admitted; one atom
    over is not. No retargeting and no dynamic adjustment of the cap. *)
Definition res_supply_plus_mint_within_cap (supply amount : Z) : bool :=
  Z.leb (supply + amount) RES_MAX_SUPPLY.

Lemma res_exact_cap_admitted :
  forall supply amount : Z,
    supply + amount = RES_MAX_SUPPLY ->
    res_supply_plus_mint_within_cap supply amount = true.
Proof.
  intros supply amount Heq.
  unfold res_supply_plus_mint_within_cap.
  apply Z.leb_le. rewrite Heq. apply Z.le_refl.
Qed.

Lemma res_overshoot_rejected :
  forall supply amount : Z,
    RES_MAX_SUPPLY < supply + amount ->
    res_supply_plus_mint_within_cap supply amount = false.
Proof.
  intros supply amount Hlt.
  unfold res_supply_plus_mint_within_cap.
  apply Z.leb_gt. exact Hlt.
Qed.

Lemma res_overshoot_one_atom_rejected :
  forall supply amount : Z,
    supply + amount = RES_MAX_SUPPLY + 1 ->
    res_supply_plus_mint_within_cap supply amount = false.
Proof.
  intros supply amount Heq.
  apply res_overshoot_rejected. rewrite Heq. apply Z.lt_succ_diag_r.
Qed.

Lemma terminated_schedule_zero_fits_cap :
  forall supply : Z,
    supply <= RES_MAX_SUPPLY ->
    res_supply_plus_mint_within_cap supply 0 = true.
Proof.
  intros supply Hle.
  unfold res_supply_plus_mint_within_cap.
  apply Z.leb_le. lia.
Qed.

(** Atom-level pin: landing exactly on the cap is allowed; one atom past it
    is not. The replay layer lifts the same pair onto [apply_block]. *)
Theorem res_cap_atom_boundaries :
  (forall supply amount : Z,
     supply + amount = RES_MAX_SUPPLY ->
     res_supply_plus_mint_within_cap supply amount = true) /\
  (forall supply amount : Z,
     supply + amount = RES_MAX_SUPPLY + 1 ->
     res_supply_plus_mint_within_cap supply amount = false).
Proof.
  split.
  - exact res_exact_cap_admitted.
  - exact res_overshoot_one_atom_rejected.
Qed.

(** ** Epoch boundaries

    [W] is constant on each epoch and jumps at [k * RES_REWARD_EPOCH].
    The last paying height still has [W(250) > 0]; the next height is [W = 0]. *)

Lemma epoch_start_div :
  forall k : nat,
    (Z.of_nat k * RES_REWARD_EPOCH) / RES_REWARD_EPOCH = Z.of_nat k.
Proof.
  intros k. unfold RES_REWARD_EPOCH. apply Z.div_mul. lia.
Qed.

Lemma get_res_mint_reward_epoch_start :
  forall k : nat,
    get_res_mint_reward (Z.of_nat k * RES_REWARD_EPOCH) = res_reward_for_epoch k.
Proof.
  intros k.
  unfold get_res_mint_reward.
  assert (Hnn : 0 <= Z.of_nat k * RES_REWARD_EPOCH).
  { unfold RES_REWARD_EPOCH. pose proof (Nat2Z.is_nonneg k). nia. }
  replace (Z.ltb (Z.of_nat k * RES_REWARD_EPOCH) 0) with false
    by (symmetry; apply Z.ltb_ge; exact Hnn).
  rewrite epoch_start_div. rewrite Nat2Z.id. reflexivity.
Qed.

Lemma epoch_last_div :
  forall k : nat,
    (Z.of_nat (S k) * RES_REWARD_EPOCH - 1) / RES_REWARD_EPOCH = Z.of_nat k.
Proof.
  intros k. unfold RES_REWARD_EPOCH.
  set (E := 210000%Z).
  set (a := Z.of_nat (S k) * E - 1).
  assert (HE : 0 < E) by (unfold E; lia).
  assert (Hlo : E * Z.of_nat k <= a).
  { unfold a, E. rewrite Nat2Z.inj_succ. nia. }
  assert (Hhi : a < E * Z.of_nat (S k)).
  { unfold a, E. nia. }
  apply Z.le_antisymm.
  - apply Z.lt_succ_r.
    replace (Z.succ (Z.of_nat k)) with (Z.of_nat (S k))
      by (rewrite Nat2Z.inj_succ; reflexivity).
    apply Z.div_lt_upper_bound; [exact HE | exact Hhi].
  - apply Z.div_le_lower_bound; [exact HE | exact Hlo].
Qed.

Lemma get_res_mint_reward_epoch_last :
  forall k : nat,
    get_res_mint_reward (Z.of_nat (S k) * RES_REWARD_EPOCH - 1) =
      res_reward_for_epoch k.
Proof.
  intros k.
  unfold get_res_mint_reward.
  assert (Hnn : 0 <= Z.of_nat (S k) * RES_REWARD_EPOCH - 1).
  { unfold RES_REWARD_EPOCH. pose proof (Nat2Z.is_nonneg (S k)). nia. }
  replace (Z.ltb (Z.of_nat (S k) * RES_REWARD_EPOCH - 1) 0) with false
    by (symmetry; apply Z.ltb_ge; exact Hnn).
  rewrite epoch_last_div. rewrite Nat2Z.id. reflexivity.
Qed.

Lemma reward_crosses_first_epoch_boundary :
  get_res_mint_reward (RES_REWARD_EPOCH - 1) = RES_REWARD_GENESIS /\
  get_res_mint_reward RES_REWARD_EPOCH = res_reward_for_epoch 1%nat /\
  get_res_mint_reward RES_REWARD_EPOCH < get_res_mint_reward (RES_REWARD_EPOCH - 1).
Proof.
  assert (Hlast : get_res_mint_reward (RES_REWARD_EPOCH - 1) = RES_REWARD_GENESIS).
  { change (RES_REWARD_EPOCH - 1) with (Z.of_nat 1 * RES_REWARD_EPOCH - 1).
    rewrite get_res_mint_reward_epoch_last. apply reward_at_genesis. }
  assert (Hstart : get_res_mint_reward RES_REWARD_EPOCH = res_reward_for_epoch 1%nat).
  { change RES_REWARD_EPOCH with (Z.of_nat 1 * RES_REWARD_EPOCH).
    apply get_res_mint_reward_epoch_start. }
  split; [exact Hlast|].
  split; [exact Hstart|].
  rewrite Hlast.
  pose proof reward_decays_across_the_first_epoch as Hd.
  rewrite reward_at_height_zero in Hd. exact Hd.
Qed.

Lemma reward_crosses_last_epoch_boundary :
  0 < get_res_mint_reward (Z.of_nat RES_REWARD_LAST_EPOCH * RES_REWARD_EPOCH) /\
  0 < get_res_mint_reward (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH - 1) /\
  get_res_mint_reward (Z.of_nat (S RES_REWARD_LAST_EPOCH) * RES_REWARD_EPOCH) = 0.
Proof.
  split; [|split].
  - rewrite get_res_mint_reward_epoch_start. apply reward_at_last_epoch_is_positive.
  - rewrite get_res_mint_reward_epoch_last. apply reward_at_last_epoch_is_positive.
  - rewrite get_res_mint_reward_epoch_start. apply reward_after_last_epoch_is_zero.
Qed.

(** ** Differential pins (C++ / Python / Coq)

    Values below are the shared RFC-017 table for the final-verification gate.
    Changing any pin without regenerating C++ [static_assert]s and the Python
    oracle is a consensus / documentation break. *)

Lemma reward_pin_table :
  res_reward_for_epoch 0%nat = 40000000 /\
  res_reward_for_epoch 1%nat = 14142196 /\
  res_reward_for_epoch 10%nat = 1096405 /\
  res_reward_for_epoch 100%nat = 39407 /\
  res_reward_for_epoch 250%nat = 10058 /\
  res_reward_for_epoch 251%nat = 0.
Proof. repeat split; vm_compute; reflexivity. Qed.

(** ** Reward / cost-floor monotonicity

    Paying epochs are pairwise nonincreasing under [vm_compute] of all 250
    adjacent pairs. With constant mint cost C > 0, W(k+1) <= W(k) and W(k+1) > 0
    imply C / W(k+1) >= C / W(k) (floor division), i.e. the ORU-per-RES cost
    floor weakly rises for the whole horizon. *)

Require Import Coq.Bool.Bool.

Definition reward_pair_nonincreasing (k : nat) : bool :=
  Z.leb (res_reward_for_epoch (S k)) (res_reward_for_epoch k).

Definition reward_nonincreasing_check : bool :=
  forallb reward_pair_nonincreasing (seq 0 RES_REWARD_LAST_EPOCH).

Lemma reward_nonincreasing_check_true : reward_nonincreasing_check = true.
Proof. vm_compute. reflexivity. Qed.

Theorem reward_nonincreasing :
  forall k : nat,
    (k < RES_REWARD_LAST_EPOCH)%nat ->
    res_reward_for_epoch (S k) <= res_reward_for_epoch k.
Proof.
  intros k Hk.
  assert (Hb : reward_pair_nonincreasing k = true).
  { assert (Hall := reward_nonincreasing_check_true).
    unfold reward_nonincreasing_check in Hall.
    assert (Hfa :=
      proj1 (forallb_forall reward_pair_nonincreasing
               (seq 0 RES_REWARD_LAST_EPOCH)) Hall).
    apply Hfa. apply in_seq. lia. }
  unfold reward_pair_nonincreasing in Hb.
  apply Z.leb_le in Hb. exact Hb.
Qed.

(** Constant positive C: smaller positive reward raises the floor quotient. *)
Lemma cost_floor_rises_when_reward_falls :
  forall (C W W' : Z),
    0 < C ->
    0 < W' ->
    W' <= W ->
    (C / W) <= (C / W').
Proof.
  intros C W W' HC HW' Hle.
  apply Z.div_le_compat_l; lia.
Qed.

Theorem cost_floor_nondecreasing_along_schedule :
  forall (C k : nat),
    (0 < Z.of_nat C)%Z ->
    (k < RES_REWARD_LAST_EPOCH)%nat ->
    0 < res_reward_for_epoch (S k) ->
    (Z.of_nat C / res_reward_for_epoch k) <=
      (Z.of_nat C / res_reward_for_epoch (S k)).
Proof.
  intros C k HC Hk HW'.
  pose proof (reward_nonincreasing k Hk) as Hrew.
  apply cost_floor_rises_when_reward_falls; lia.
Qed.
