(** Issuance height boundary (P0-040), RFC-020.

      observational calendar(h) = floor(h / 210000)
      ORU(0)   = 0
      ORU(h)   = 128 ORU                         for h >= 1
      destroy_cost(h) = P0 · (2^25 + h)^2 / 2^50  continuous; not an epoch step

    There is no halving. [CALENDAR_INTERVAL] is an observer bucket.
    [HALVING_INTERVAL] is a leftover name for that bucket.

    No [Axiom] / [Admitted] / [Parameter]. Not a ConnectBlock refinement. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition CALENDAR_INTERVAL : Z := 210000.
Definition HALVING_INTERVAL : Z := CALENDAR_INTERVAL.

Definition ORU_FLAT_SATS : Z := ORU_FLAT_BLOCK_SUBSIDY.
Definition ORU_INITIAL_SATS : Z := ORU_FLAT_SATS.

Definition issuance_epoch (h : Z) : Z :=
  if h <? 0 then 0 else h / CALENDAR_INTERVAL.

Definition issuance_epoch_oru : Z -> Z := issuance_epoch.
Definition issuance_epoch_res : Z -> Z := issuance_epoch.

Definition oru_subsidy (h : Z) : Z :=
  if h <=? 0 then 0 else ORU_FLAT_SATS.

Definition res_destroy_cost (h : Z) : Z := res_destroy_cost_at_height h.
Definition res_subsidy (h : Z) : Z := res_destroy_cost h.

Theorem oru_subsidy_constant :
  oru_subsidy 0 = 0 /\
  forall h, 1 <= h -> oru_subsidy h = ORU_FLAT_SATS.
Proof.
  split; [reflexivity|].
  intros h Hh.
  unfold oru_subsidy.
  destruct (Z.leb_spec h 0); [lia | reflexivity].
Qed.

Theorem oru_res_calendar_agree :
  forall h, issuance_epoch_oru h = issuance_epoch_res h.
Proof. intros. reflexivity. Qed.

Theorem oru_res_halving_boundary_agree :
  forall h, issuance_epoch_oru h = issuance_epoch_res h.
Proof. apply oru_res_calendar_agree. Qed.

Theorem genesis_reward_zero :
  oru_subsidy 0 = 0.
Proof. apply (proj1 oru_subsidy_constant). Qed.

Theorem first_issuance_height :
  oru_subsidy 0 = 0 /\ oru_subsidy 1 = ORU_FLAT_SATS.
Proof. split; [reflexivity | apply (proj2 oru_subsidy_constant); lia]. Qed.

Theorem no_height_1_compensation :
  oru_subsidy 1 <> ORU_FLAT_SATS + ORU_FLAT_SATS.
Proof. discriminate. Qed.

Theorem epoch_0_on_genesis_and_height_1 :
  issuance_epoch 0 = 0 /\ issuance_epoch 1 = 0.
Proof. split; reflexivity. Qed.

Theorem first_calendar_boundary :
  issuance_epoch 209999 = 0 /\
  issuance_epoch 210000 = 1 /\
  issuance_epoch 210001 = 1.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem second_calendar_boundary :
  issuance_epoch 419999 = 1 /\
  issuance_epoch 420000 = 2 /\
  issuance_epoch 420001 = 2.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem third_calendar_boundary :
  issuance_epoch 629999 = 2 /\
  issuance_epoch 630000 = 3 /\
  issuance_epoch 630001 = 3.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem first_halving_height :
  issuance_epoch 209999 = 0 /\
  issuance_epoch 210000 = 1 /\
  issuance_epoch 210001 = 1.
Proof. apply first_calendar_boundary. Qed.

Theorem second_halving_height :
  issuance_epoch 419999 = 1 /\
  issuance_epoch 420000 = 2 /\
  issuance_epoch 420001 = 2.
Proof. apply second_calendar_boundary. Qed.

Theorem third_halving_height :
  issuance_epoch 629999 = 2 /\
  issuance_epoch 630000 = 3 /\
  issuance_epoch 630001 = 3.
Proof. apply third_calendar_boundary. Qed.

Theorem oru_is_flat_after_genesis :
  oru_subsidy 1 = ORU_FLAT_SATS /\
  oru_subsidy 209999 = ORU_FLAT_SATS /\
  oru_subsidy 210000 = ORU_FLAT_SATS /\
  oru_subsidy 420000 = ORU_FLAT_SATS /\
  oru_subsidy 630000 = ORU_FLAT_SATS.
Proof. repeat split; reflexivity. Qed.

Theorem height_210000_eq_209999 :
  oru_subsidy 210000 = oru_subsidy 209999.
Proof. reflexivity. Qed.

Theorem res_is_continuous_destroy_cost :
  forall h, res_subsidy h = res_destroy_cost_at_height h.
Proof. intros. reflexivity. Qed.

Theorem golden_height_0 :
  oru_subsidy 0 = 0 /\
  res_subsidy 0 = RES_DESTROY_COST_GENESIS /\
  issuance_epoch 0 = 0.
Proof. repeat split; reflexivity. Qed.

Theorem golden_height_1 :
  oru_subsidy 1 = ORU_FLAT_SATS /\
  res_subsidy 1 = 102400006103 /\
  issuance_epoch 1 = 0.
Proof. repeat split; vm_compute; reflexivity. Qed.

Theorem golden_height_210000_oru_epoch :
  oru_subsidy 210000 = ORU_FLAT_SATS /\
  issuance_epoch 210000 = 1.
Proof. split; reflexivity. Qed.

Theorem golden_height_210000_res :
  res_subsidy 210000 = 103685749152.
Proof. vm_compute. reflexivity. Qed.

Theorem golden_height_420000_oru_epoch :
  oru_subsidy 420000 = ORU_FLAT_SATS /\
  issuance_epoch 420000 = 2.
Proof. split; reflexivity. Qed.

Theorem golden_height_420000_res :
  res_subsidy 420000 = 104979520049.
Proof. vm_compute. reflexivity. Qed.

Definition cpp_refinement_proved : bool := false.

Theorem issuance_boundary_formula_is_height_div_interval :
  forall h, 0 <= h -> issuance_epoch h = h / 210000.
Proof.
  intros h Hh.
  unfold issuance_epoch, CALENDAR_INTERVAL.
  destruct (Z.ltb_spec h 0); [lia | reflexivity].
Qed.
