(** ResSchedule.v — RFC-020 algebraic issuance model.

    Numeric SSOT: `tools/res_continuous_price_schedule.py`.
    C++ mirror: `core/src/consensus/res_issuance.h`.

    Consensus never evaluates the display destroy cost. It evaluates one
    inequality, with no division, no floor and no square root:

      R > 0  →  R · (2^25 + h)^2 ≤ B << 40
      R = 0  →  B unconstrained (a plain burn stays legal)

    Theorem names match the Python SSOT assertions identically.

    No [Axiom] / [Admitted] / [Parameter]. Not a C++ source refinement. *)

Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Require Import Coq.Bool.Bool.
Require Import Coq.micromega.Lia.
Import ListNotations.

Local Open Scope Z_scope.

(** ** Constants — forced by RFC-020, not free parameters *)

Definition RES_COIN : Z := 100000000.
Definition COIN : Z := 100000000.
Definition RES_DESTROY_COST_GENESIS : Z := 1024 * COIN.
Definition RES_DESTROY_POLE : Z := Z.shiftl 1 25.
Definition RES_DESTROY_SHIFT : Z := 40.
Definition RES_MAX_SUPPLY : Z := Z.shiftl RES_COIN 22.
Definition MAX_MONEY : Z := 10000000000 * COIN.
Definition ORU_FLAT_BLOCK_SUBSIDY : Z := 128 * COIN.
Definition INT32_MAX : Z := Z.shiftl 1 31 - 1.
Definition UINT128_MAX : Z := Z.shiftl 1 128 - 1.

(** Observational calendar only. Not a mint argument. *)
Definition RES_REWARD_EPOCH : Z := 210000.

(** ** Display destroy cost (not a consensus input)

    destroy_cost(h) = P0 · (H + h)^2 / 2^50, floor-divided as C++ `>> 50`. *)

Definition res_destroy_pole_sq (h : Z) : Z :=
  (RES_DESTROY_POLE + h) * (RES_DESTROY_POLE + h).

Definition res_destroy_cost_at_height (height : Z) : Z :=
  if height <? 0 then 0
  else Z.shiftr (RES_DESTROY_COST_GENESIS * res_destroy_pole_sq height) 50.

Definition res_mint_authorized (height : Z) : bool := 0 <=? height.

(** Wallet inverse: largest R such that the pole predicate holds. *)
Definition res_max_mint_for_burn (burn_atoms height : Z) : Z :=
  if (burn_atoms <=? 0) || (height <? 0) then 0
  else Z.shiftl burn_atoms RES_DESTROY_SHIFT / res_destroy_pole_sq height.

(** Smallest B that mints R atoms: ceil(R · (H+h)^2 / 2^40). *)
Definition res_min_burn_for_atoms (res_atoms height : Z) : Z :=
  if (res_atoms <=? 0) || (height <? 0) then 0
  else
    let numer := res_atoms * res_destroy_pole_sq height in
    let den := Z.shiftl 1 RES_DESTROY_SHIFT in
    (numer + den - 1) / den.

Definition min_burn_for_one_atom (height : Z) : Z :=
  res_min_burn_for_atoms 1 height.

(** ** Closed-form cap

    cap = (S / P0) · H = 2^7 / 2^10 · 2^25 = 2^22 RES.
    Antiderivative of the per-block ceiling: cap · h / (h + H). *)

Definition cumulative_res_at (height : Z) : Z :=
  if height <=? 0 then 0
  else RES_MAX_SUPPLY * height / (height + RES_DESTROY_POLE).

(** ** Mint arithmetic (RFC-020 §2) *)

Definition check_res_mint_arith (res_atoms burn_atoms height : Z) : bool :=
  if ((res_atoms <? 0) || (burn_atoms <? 0) || (height <? 0))%bool then false
  else if res_atoms =? 0 then true
  else if RES_MAX_SUPPLY <? res_atoms then false
  else if MAX_MONEY <? burn_atoms then false
  else res_atoms * res_destroy_pole_sq height <=? Z.shiftl burn_atoms RES_DESTROY_SHIFT.

(** Observational calendar sample of [destroy_cost(h)]. Not a mint argument. *)

Definition res_destroy_cost_for_epoch (k : nat) : Z :=
  res_destroy_cost_at_height (Z.of_nat k * RES_REWARD_EPOCH).

(** ** Supporting facts *)

Lemma genesis_destroy_cost_pos : 0 < RES_DESTROY_COST_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma pole_value : RES_DESTROY_POLE = 33554432.
Proof. vm_compute. reflexivity. Qed.

Lemma pole_pos : 0 < RES_DESTROY_POLE.
Proof. vm_compute. reflexivity. Qed.

Lemma pole_sq_pos :
  forall h, 0 <= h -> 0 < res_destroy_pole_sq h.
Proof.
  intros h Hh. unfold res_destroy_pole_sq.
  pose proof pole_pos. nia.
Qed.

Lemma shiftr50_mono :
  forall a b, 0 <= a <= b -> Z.shiftr a 50 <= Z.shiftr b 50.
Proof.
  intros a b H.
  rewrite !Z.shiftr_div_pow2 by lia.
  apply Z.div_le_mono; lia.
Qed.

Lemma res_destroy_cost_at_height_nonneg :
  forall h : Z, 0 <= res_destroy_cost_at_height h.
Proof.
  intros h. unfold res_destroy_cost_at_height.
  destruct (Z.ltb_spec h 0); [apply Z.le_refl|].
  apply Z.shiftr_nonneg.
  pose proof genesis_destroy_cost_pos.
  pose proof pole_pos.
  unfold res_destroy_pole_sq. nia.
Qed.

Lemma res_destroy_cost_at_height_mono :
  forall h1 h2,
    0 <= h1 <= h2 ->
    res_destroy_cost_at_height h1 <= res_destroy_cost_at_height h2.
Proof.
  intros h1 h2 Hh.
  unfold res_destroy_cost_at_height, res_destroy_pole_sq.
  rewrite (proj2 (Z.ltb_ge h1 0)) by lia.
  rewrite (proj2 (Z.ltb_ge h2 0)) by lia.
  apply shiftr50_mono.
  pose proof genesis_destroy_cost_pos.
  rewrite pole_value.
  unfold RES_DESTROY_COST_GENESIS, COIN.
  nia.
Qed.

(** ** RFC-020 §8 theorems — names match the Python SSOT *)

Theorem predicate_shift_is_exact :
  RES_DESTROY_COST_GENESIS / COIN = 1024 /\
  RES_DESTROY_SHIFT = 50 - 10.
Proof. split; vm_compute; reflexivity. Qed.

Theorem genesis_destroy_cost_is_exact :
  res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS /\
  res_destroy_cost_at_height 0 = 1024 * COIN /\
  min_burn_for_one_atom 0 = 1024.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem destroy_cost_genesis_exact :
  res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS /\
  min_burn_for_one_atom 0 = 1024.
Proof.
  destruct genesis_destroy_cost_is_exact as [H0 [H1 H2]].
  split; [exact H0 | exact H2].
Qed.

Theorem destroy_cost_strictly_increasing :
  (forall h, 0 <= h ->
     res_destroy_cost_at_height h <= res_destroy_cost_at_height (h + 1)) /\
  res_destroy_cost_at_height (Z.shiftl 1 20) > res_destroy_cost_at_height 0.
Proof.
  split.
  - intros h Hh.
    apply res_destroy_cost_at_height_mono. lia.
  - vm_compute. reflexivity.
Qed.

Lemma check_one_atom_zero_burn_false :
  forall h, 0 <= h -> check_res_mint_arith 1 0 h = false.
Proof.
  intros h Hh.
  unfold check_res_mint_arith.
  assert (Hneg : ((1 <? 0) || (0 <? 0) || (h <? 0))%bool = false).
  { rewrite (proj2 (Z.ltb_ge h 0)) by lia. reflexivity. }
  rewrite Hneg.
  change (1 =? 0) with false.
  change (RES_MAX_SUPPLY <? 1) with false.
  change (MAX_MONEY <? 0) with false.
  apply Z.leb_gt.
  change (Z.shiftl 0 RES_DESTROY_SHIFT) with 0.
  unfold res_destroy_pole_sq.
  pose proof pole_pos. nia.
Qed.

Lemma min_burn_one_atom_pos :
  forall h, 0 <= h -> 1 <= min_burn_for_one_atom h.
Proof.
  intros h Hh.
  unfold min_burn_for_one_atom, res_min_burn_for_atoms.
  change (1 <=? 0) with false.
  rewrite (proj2 (Z.ltb_ge h 0)) by lia.
  change (false || false)%bool with false.
  pose proof (pole_sq_pos h Hh) as Hsq.
  apply Z.div_le_lower_bound.
  - vm_compute. reflexivity.
  - assert (1 <= res_destroy_pole_sq h) by lia.
    unfold res_destroy_pole_sq in *. lia.
Qed.

Theorem no_free_mint_anywhere :
  forall h,
    0 <= h ->
    check_res_mint_arith 1 0 h = false /\
    0 < res_destroy_cost_at_height h /\
    1 <= min_burn_for_one_atom h.
Proof.
  intros h Hh.
  split; [apply check_one_atom_zero_burn_false; exact Hh|].
  split.
  - apply Z.lt_le_trans with (m := RES_DESTROY_COST_GENESIS).
    + vm_compute. reflexivity.
    + rewrite <- (proj1 genesis_destroy_cost_is_exact).
      apply res_destroy_cost_at_height_mono. lia.
  - apply min_burn_one_atom_pos; exact Hh.
Qed.

Theorem no_free_mint :
  forall h, 0 <= h -> check_res_mint_arith 1 0 h = false.
Proof. intros h Hh. apply (proj1 (no_free_mint_anywhere h Hh)). Qed.

Theorem no_overflow_in_uint128 :
  RES_MAX_SUPPLY * res_destroy_pole_sq INT32_MAX <= UINT128_MAX /\
  Z.shiftl MAX_MONEY RES_DESTROY_SHIFT <= UINT128_MAX.
Proof. split; apply Z.leb_le; vm_compute; reflexivity. Qed.

Theorem mint_arithmetic_no_overflow :
  RES_MAX_SUPPLY * res_destroy_pole_sq INT32_MAX <= UINT128_MAX /\
  Z.shiftl MAX_MONEY RES_DESTROY_SHIFT <= UINT128_MAX.
Proof. apply no_overflow_in_uint128. Qed.

Theorem cap_is_closed_form :
  RES_MAX_SUPPLY = Z.shiftl RES_COIN 22 /\
  (ORU_FLAT_BLOCK_SUBSIDY / RES_DESTROY_COST_GENESIS * RES_DESTROY_POLE)
    * RES_COIN = 0 /\
  (128 * RES_DESTROY_POLE / 1024) * RES_COIN = RES_MAX_SUPPLY.
Proof.
  split; [reflexivity|].
  split; vm_compute; reflexivity.
Qed.

Theorem cap_closed_form_exact :
  (128 * RES_DESTROY_POLE / 1024) * RES_COIN = RES_MAX_SUPPLY.
Proof. apply (proj2 (proj2 cap_is_closed_form)). Qed.

Theorem cap_is_upper_bound :
  forall h, 0 <= h -> cumulative_res_at h <= RES_MAX_SUPPLY.
Proof.
  intros h Hh.
  unfold cumulative_res_at.
  destruct (Z.leb_spec h 0).
  - change RES_MAX_SUPPLY with 419430400000000. lia.
  - apply Z.div_le_upper_bound.
    + pose proof pole_pos. lia.
    + change RES_MAX_SUPPLY with 419430400000000.
      pose proof pole_pos.
      nia.
Qed.

Lemma cap_is_two_to_the_twenty_two_res :
  RES_MAX_SUPPLY = Z.shiftl RES_COIN 22.
Proof. reflexivity. Qed.

Lemma cap_from_flat_subsidy_and_pole :
  (128 * RES_DESTROY_POLE / 1024) * RES_COIN = RES_MAX_SUPPLY.
Proof. apply cap_closed_form_exact. Qed.

(** Display destroy cost and the pole inverse differ by at most one atom
    at the pinned heights the Python SSOT checks. *)
Lemma min_burn_one_res_genesis :
  res_min_burn_for_atoms RES_COIN 0 = RES_DESTROY_COST_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma min_burn_one_res_height_1 :
  res_min_burn_for_atoms RES_COIN 1 = 102400006104.
Proof. vm_compute. reflexivity. Qed.

Lemma genesis_one_res_at_exact_destroy_cost :
  check_res_mint_arith RES_COIN RES_DESTROY_COST_GENESIS 0 = true.
Proof. vm_compute. reflexivity. Qed.

Lemma genesis_one_res_one_atom_short :
  check_res_mint_arith RES_COIN (RES_DESTROY_COST_GENESIS - 1) 0 = false.
Proof. vm_compute. reflexivity. Qed.

Lemma height_1_one_res_at_min_burn :
  check_res_mint_arith RES_COIN 102400006104 1 = true.
Proof. vm_compute. reflexivity. Qed.

Lemma height_1_one_res_one_atom_short :
  check_res_mint_arith RES_COIN 102400006103 1 = false.
Proof. vm_compute. reflexivity. Qed.

Lemma destroy_cost_at_height_1 :
  res_destroy_cost_at_height 1 = 102400006103.
Proof. vm_compute. reflexivity. Qed.

Theorem predicate_matches_destroy_cost :
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 0) 0 = true /\
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 0 - 1) 0 = false /\
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 1) 1 = true /\
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 1 - 1) 1 = false /\
  Z.abs (res_min_burn_for_atoms RES_COIN 0 - res_destroy_cost_at_height 0) <= 1 /\
  Z.abs (res_min_burn_for_atoms RES_COIN 1 - res_destroy_cost_at_height 1) <= 1.
Proof.
  rewrite min_burn_one_res_genesis, min_burn_one_res_height_1.
  rewrite (proj1 genesis_destroy_cost_is_exact), destroy_cost_at_height_1.
  split; [apply genesis_one_res_at_exact_destroy_cost|].
  split; [apply genesis_one_res_one_atom_short|].
  split; [apply height_1_one_res_at_min_burn|].
  split; [apply height_1_one_res_one_atom_short|].
  split.
  - change (Z.abs (RES_DESTROY_COST_GENESIS - RES_DESTROY_COST_GENESIS) <= 1)
      with (0 <= 1).
    lia.
  - change (Z.abs (102400006104 - 102400006103) <= 1) with (1 <= 1).
    lia.
Qed.

Theorem predicate_equals_destroy_cost :
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 0) 0 = true /\
  check_res_mint_arith RES_COIN (res_min_burn_for_atoms RES_COIN 0 - 1) 0 = false.
Proof.
  destruct predicate_matches_destroy_cost as [H0 [H1 _]].
  split; [exact H0 | exact H1].
Qed.

(** ** Concrete pins *)

Lemma destroy_cost_at_height_zero : res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS.
Proof. apply (proj1 genesis_destroy_cost_is_exact). Qed.

Lemma destroy_cost_at_genesis : res_destroy_cost_for_epoch 0 = RES_DESTROY_COST_GENESIS.
Proof. vm_compute. reflexivity. Qed.

Lemma destroy_cost_pin_table :
  res_destroy_cost_at_height 0 = 102400000000 /\
  res_destroy_cost_at_height 1 = 102400006103 /\
  res_destroy_cost_at_height 210000 = 103685749152 /\
  res_destroy_cost_at_height 420000 = 104979520049 /\
  res_destroy_cost_for_epoch 0%nat = 102400000000 /\
  res_destroy_cost_for_epoch 1%nat = 103685749152.
Proof. repeat split; vm_compute; reflexivity. Qed.

Lemma destroy_cost_hardens_across_the_first_epoch :
  res_destroy_cost_at_height 0 < res_destroy_cost_at_height RES_REWARD_EPOCH.
Proof.
  rewrite (proj1 genesis_destroy_cost_is_exact).
  assert (Hp : res_destroy_cost_at_height RES_REWARD_EPOCH = 103685749152)
    by (vm_compute; reflexivity).
  rewrite Hp. unfold RES_DESTROY_COST_GENESIS, COIN. lia.
Qed.

Lemma plain_burn_always_ok :
  check_res_mint_arith 0 MAX_MONEY 0 = true.
Proof. vm_compute. reflexivity. Qed.

Lemma zero_burn_cannot_mint :
  check_res_mint_arith 1 0 0 = false.
Proof. vm_compute. reflexivity. Qed.

Lemma negative_height_rejected :
  check_res_mint_arith 0 1 (-1) = false.
Proof. vm_compute. reflexivity. Qed.

Lemma res_destroy_cost_for_epoch_nonneg :
  forall k : nat, 0 <= res_destroy_cost_for_epoch k.
Proof. intros k. apply res_destroy_cost_at_height_nonneg. Qed.

Theorem destroy_cost_epoch_nondecreasing :
  forall k : nat,
    res_destroy_cost_for_epoch k <= res_destroy_cost_for_epoch (S k).
Proof.
  intros k.
  unfold res_destroy_cost_for_epoch.
  apply res_destroy_cost_at_height_mono.
  unfold RES_REWARD_EPOCH.
  pose proof (Nat2Z.is_nonneg k).
  rewrite Nat2Z.inj_succ.
  nia.
Qed.

Lemma destroy_cost_never_zero_nonneg :
  forall h, 0 <= h -> 0 < res_destroy_cost_at_height h.
Proof.
  intros h Hh.
  apply (proj1 (proj2 (no_free_mint_anywhere h Hh))).
Qed.

(** ** Runtime cap-fit *)

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
