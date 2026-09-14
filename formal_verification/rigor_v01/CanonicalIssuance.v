(** Canonical issuance supply (P0-041), RFC-020.

    Reuses [IssuanceHeightBoundary] calendar / flat ORU / continuous RES price.
    Cumulative ORU starts at height 1 (genesis excluded):

      Σ_{h=1}^{n} ORU = n · 128 ORU

    There is no per-height RES grant. Minting is body-tx burn-to-mint.
    [res_issued_at_height] is identically zero.

    No [Axiom] / [Admitted] / [Parameter]. Not a C++ ConnectBlock refinement. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorV01 Require Import IssuanceHeightBoundary.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition epoch : Z -> Z := issuance_epoch.
Definition oru_reward_at_height : Z -> Z := oru_subsidy.
Definition res_destroy_cost_at_height : Z -> Z := res_subsidy.

Definition ORU_R0 : Z := ORU_FLAT_SATS.
Definition ORU_COIN : Z := 100000000.

Definition RES_P0 : Z := 102400000000.
Definition RES_P_H210000 : Z := 103685749152.
Definition RES_P_H420000 : Z := 104979520049.

(** Actual per-height RES grant is zero. Minting is body-tx burn-to-mint. *)
Definition res_issued_at_height (_ : Z) : Z := 0.

Theorem res_issued_zero : res_issued_at_height 0 = 0.
Proof. reflexivity. Qed.

Theorem epoch_0 : epoch 0 = 0.
Proof. reflexivity. Qed.

Theorem epoch_1 : epoch 1 = 0.
Proof. reflexivity. Qed.

Theorem epoch_209999 : epoch 209999 = 0.
Proof. reflexivity. Qed.

Theorem epoch_210000 : epoch 210000 = 1.
Proof. reflexivity. Qed.

Theorem epoch_210001 : epoch 210001 = 1.
Proof. reflexivity. Qed.

Theorem epoch_419999 : epoch 419999 = 1.
Proof. reflexivity. Qed.

Theorem epoch_420000 : epoch 420000 = 2.
Proof. reflexivity. Qed.

Theorem epoch_formula :
  forall h, 0 <= h -> epoch h = h / 210000.
Proof. apply issuance_boundary_formula_is_height_div_interval. Qed.

Theorem oru_res_same_epoch :
  forall h, issuance_epoch_oru h = issuance_epoch_res h.
Proof. apply oru_res_calendar_agree. Qed.

Theorem first_calendar_bucket_boundary :
  epoch 209999 = 0 /\ epoch 210000 = 1.
Proof. split; reflexivity. Qed.

Theorem first_halving_height_is_210000 :
  epoch 209999 = 0 /\ epoch 210000 = 1.
Proof. apply first_calendar_bucket_boundary. Qed.

Theorem flat_oru_across_first_calendar_boundary :
  epoch 210000 = 1 /\ epoch 210001 = 1 /\
  oru_reward_at_height 210000 = oru_reward_at_height 210001.
Proof. repeat split; reflexivity. Qed.

Theorem first_halving_is_not_210001 :
  epoch 210000 = 1 /\ epoch 210001 = 1 /\
  oru_reward_at_height 210000 = oru_reward_at_height 210001.
Proof. apply flat_oru_across_first_calendar_boundary. Qed.

Theorem oru_reward_genesis : oru_reward_at_height 0 = 0.
Proof. reflexivity. Qed.

Theorem oru_reward_first_issuance : oru_reward_at_height 1 = ORU_R0.
Proof. reflexivity. Qed.

Theorem oru_is_flat :
  forall h, 1 <= h -> oru_reward_at_height h = ORU_R0.
Proof. apply (proj2 oru_subsidy_constant). Qed.

Theorem no_genesis_compensation_at_height_1 :
  oru_reward_at_height 1 = ORU_R0 /\
  oru_reward_at_height 1 <> ORU_R0 + ORU_R0.
Proof. split; [reflexivity|discriminate]. Qed.

Theorem oru_reward_is_frozen_cpp_schedule :
  forall h, 0 <= h ->
    oru_reward_at_height h = (if h <=? 0 then 0 else ORU_FLAT_SATS).
Proof. intros h Hh. unfold oru_reward_at_height, oru_subsidy. reflexivity. Qed.

Lemma leb_false : forall a b, b < a -> (a <=? b) = false.
Proof. intros. apply Z.leb_gt. lia. Qed.

(** * Cumulative ORU — recursive sum from height 1, plus closed form n · 128 *)

Fixpoint total_oru_issued_nat (n : nat) : Z :=
  match n with
  | O => 0
  | S k => total_oru_issued_nat k + oru_reward_at_height (Z.of_nat (S k))
  end.

Definition total_oru_issued (H : Z) : Z :=
  if H <=? 0 then 0 else total_oru_issued_nat (Z.to_nat H).

Definition total_oru_issued_closed (H : Z) : Z :=
  if H <=? 0 then 0 else H * ORU_R0.

Lemma closed_oru_nonpos :
  forall H, H <= 0 -> total_oru_issued_closed H = 0.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite (proj2 (Z.leb_le H 0)) by lia. reflexivity.
Qed.

Lemma closed_oru_pos :
  forall H, 0 < H -> total_oru_issued_closed H = H * ORU_R0.
Proof.
  intros H Hh. unfold total_oru_issued_closed.
  rewrite leb_false by lia. reflexivity.
Qed.

Lemma total_oru_closed_step :
  forall h, 0 < h ->
    total_oru_issued_closed (h - 1) + oru_reward_at_height h =
    total_oru_issued_closed h.
Proof.
  intros h Hh.
  rewrite oru_is_flat by lia.
  destruct (Z.le_gt_cases h 1) as [H1|H1].
  - assert (h = 1) by lia. subst h.
    rewrite closed_oru_nonpos by lia. rewrite closed_oru_pos by lia. lia.
  - rewrite closed_oru_pos by lia. rewrite closed_oru_pos by lia. lia.
Qed.

Theorem total_oru_issued_nat_closed :
  forall n, total_oru_issued_nat n = total_oru_issued_closed (Z.of_nat n).
Proof.
  induction n as [|k IH].
  - reflexivity.
  - simpl. rewrite IH.
    replace (Z.of_nat k) with (Z.of_nat (S k) - 1) by lia.
    apply total_oru_closed_step. lia.
Qed.

Theorem total_oru_issued_eq_closed :
  forall H, 0 <= H -> total_oru_issued H = total_oru_issued_closed H.
Proof.
  intros H Hh. unfold total_oru_issued.
  destruct (Z.leb_spec H 0).
  - rewrite closed_oru_nonpos by lia. reflexivity.
  - rewrite total_oru_issued_nat_closed. rewrite Z2Nat.id by lia. reflexivity.
Qed.

Theorem genesis_excluded_from_oru_issuance :
  total_oru_issued 0 = 0 /\ oru_reward_at_height 0 = 0.
Proof. split; reflexivity. Qed.

Theorem cumulative_oru_height_1 : total_oru_issued 1 = ORU_R0.
Proof. reflexivity. Qed.

Theorem cumulative_oru_is_height_times_flat :
  forall n, 0 <= n -> total_oru_issued n = n * ORU_R0 \/ n = 0.
Proof.
  intros n Hn.
  destruct (Z.eq_dec n 0) as [Z0|Znz].
  - right. exact Z0.
  - left. rewrite total_oru_issued_eq_closed by lia.
    rewrite closed_oru_pos; lia.
Qed.

Theorem first_epoch_oru_atoms :
  total_oru_issued 209999 = 2687987200000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_pos by lia.
  vm_compute. reflexivity.
Qed.

Theorem first_epoch_oru_coins :
  total_oru_issued 209999 / ORU_COIN = 26879872.
Proof.
  rewrite first_epoch_oru_atoms. vm_compute. reflexivity.
Qed.

Theorem through_first_calendar_boundary_oru_atoms :
  total_oru_issued 210000 = 2688000000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_pos by lia.
  vm_compute. reflexivity.
Qed.

Theorem through_first_halving_oru_atoms :
  total_oru_issued 210000 = 2688000000000000.
Proof. apply through_first_calendar_boundary_oru_atoms. Qed.

Theorem through_first_halving_oru_coins :
  total_oru_issued 210000 / ORU_COIN = 26880000.
Proof.
  rewrite through_first_calendar_boundary_oru_atoms. vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_210001 :
  total_oru_issued 210001 = 2688012800000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_pos by lia.
  vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_420000 :
  total_oru_issued 420000 = 5376000000000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_pos by lia.
  vm_compute. reflexivity.
Qed.

Theorem cumulative_oru_420001 :
  total_oru_issued 420001 = 5376012800000000.
Proof.
  rewrite total_oru_issued_eq_closed by lia.
  rewrite closed_oru_pos by lia.
  vm_compute. reflexivity.
Qed.

Theorem first_epoch_is_exactly_209999_blocks_of_flat :
  total_oru_issued 210000 / ORU_COIN = 26880000.
Proof. apply through_first_halving_oru_coins. Qed.

Theorem res_derived_cap_pin :
  RES_MAX_SUPPLY = Z.shiftl RES_COIN 22.
Proof. apply cap_is_two_to_the_twenty_two_res. Qed.

Theorem res_derived_cap_is_bound :
  forall h, 0 <= h -> cumulative_res_at h <= RES_MAX_SUPPLY.
Proof. apply cap_is_upper_bound. Qed.

Theorem genesis_res_destroy_cost_is_not_issuance :
  res_destroy_cost_at_height 0 = RES_DESTROY_COST_GENESIS /\
  res_destroy_cost_at_height 0 = RES_P0 /\
  res_issued_at_height 0 = 0.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem rate_hardens_at_first_calendar_boundary :
  res_destroy_cost_at_height 0 < res_destroy_cost_at_height 210000.
Proof. vm_compute. reflexivity. Qed.

Theorem rate_hardens_at_first_halving :
  res_destroy_cost_at_height 0 < res_destroy_cost_at_height 210000.
Proof. apply rate_hardens_at_first_calendar_boundary. Qed.

Theorem res_destroy_cost_pins :
  res_destroy_cost_at_height 0 = RES_P0 /\
  res_destroy_cost_at_height 210000 = RES_P_H210000 /\
  res_destroy_cost_at_height 420000 = RES_P_H420000.
Proof. repeat split; vm_compute; reflexivity. Qed.

Definition canonical_issuance_cpp_refinement_proved : bool := false.

Theorem canonical_issuance_cpp_refinement_not_proved :
  canonical_issuance_cpp_refinement_proved = false.
Proof. reflexivity. Qed.
