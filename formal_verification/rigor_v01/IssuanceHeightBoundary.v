(** Issuance height boundary (P0-040).

    Frozen C++ semantic, not the unused (h-1)/210000 proposal:

      epoch(h) = floor(h / 210000)
      ORU(0)   = 0
      ORU(h)   = 1000 ORU >> min(epoch(h), 3)   for h >= 1
      RES(h)   = W(epoch(h))                    RFC-017 schedule

    First full-reward ORU heights are 1..209999. Height 210000 is epoch 1.
    No [Axiom] / [Admitted] / [Parameter]. Not a ConnectBlock refinement. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Rigor Require Import ResSchedule.

Local Open Scope Z_scope.

Definition HALVING_INTERVAL : Z := 210000.
Definition ORU_INITIAL_SATS : Z := 100000000000.

Definition issuance_epoch (h : Z) : Z :=
  if h <? 0 then 0 else h / HALVING_INTERVAL.

Definition issuance_epoch_oru : Z -> Z := issuance_epoch.
Definition issuance_epoch_res : Z -> Z := issuance_epoch.

Definition oru_subsidy (h : Z) : Z :=
  if h =? 0 then 0
  else Z.shiftr ORU_INITIAL_SATS (Z.min (issuance_epoch h) 3).

Definition res_subsidy (h : Z) : Z := get_res_mint_reward h.

Theorem oru_res_halving_boundary_agree :
  forall h, issuance_epoch_oru h = issuance_epoch_res h.
Proof. intros. reflexivity. Qed.

Theorem genesis_reward_zero :
  oru_subsidy 0 = 0.
Proof. reflexivity. Qed.

Theorem first_issuance_height :
  oru_subsidy 0 = 0 /\ oru_subsidy 1 = ORU_INITIAL_SATS.
Proof. split; reflexivity. Qed.

Theorem no_height_1_compensation :
  oru_subsidy 1 <> ORU_INITIAL_SATS + ORU_INITIAL_SATS.
Proof. discriminate. Qed.

Theorem epoch_0_on_genesis_and_height_1 :
  issuance_epoch 0 = 0 /\ issuance_epoch 1 = 0.
Proof. split; reflexivity. Qed.

Theorem first_halving_height :
  issuance_epoch 209999 = 0 /\
  issuance_epoch 210000 = 1 /\
  issuance_epoch 210001 = 1.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem second_halving_height :
  issuance_epoch 419999 = 1 /\
  issuance_epoch 420000 = 2 /\
  issuance_epoch 420001 = 2.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem third_halving_height :
  issuance_epoch 629999 = 2 /\
  issuance_epoch 630000 = 3 /\
  issuance_epoch 630001 = 3.
Proof. split; [reflexivity | split; reflexivity]. Qed.

Theorem height_209999_full_oru :
  oru_subsidy 209999 = ORU_INITIAL_SATS.
Proof. reflexivity. Qed.

Theorem height_210000_halved_oru :
  oru_subsidy 210000 = Z.shiftr ORU_INITIAL_SATS 1.
Proof. reflexivity. Qed.

Theorem height_210000_ne_209999 :
  oru_subsidy 210000 <> oru_subsidy 209999.
Proof. discriminate. Qed.

Theorem height_420000_quarter_oru :
  oru_subsidy 420000 = Z.shiftr ORU_INITIAL_SATS 2.
Proof. reflexivity. Qed.

Theorem height_630000_floor_oru :
  oru_subsidy 630000 = Z.shiftr ORU_INITIAL_SATS 3.
Proof. reflexivity. Qed.

(** First full-reward ORU epoch is 209999 blocks (1..209999), not 210000. *)
Theorem first_full_oru_epoch_last_height :
  issuance_epoch 209999 = 0 /\ issuance_epoch 210000 = 1.
Proof. split; reflexivity. Qed.

Theorem res_follows_same_epoch :
  forall h, 0 <= h -> res_subsidy h = res_reward_for_epoch (Z.to_nat (issuance_epoch h)).
Proof.
  intros h Hh. unfold res_subsidy, get_res_mint_reward, issuance_epoch.
  destruct (Z.ltb h 0) eqn:L; [apply Z.ltb_lt in L; lia |].
  destruct (h <? 0) eqn:L2; [discriminate |].
  reflexivity.
Qed.

Theorem golden_height_0 :
  oru_subsidy 0 = 0 /\
  res_subsidy 0 = RES_REWARD_GENESIS /\
  issuance_epoch 0 = 0.
Proof. repeat split; reflexivity. Qed.

Theorem golden_height_1 :
  oru_subsidy 1 = ORU_INITIAL_SATS /\
  res_subsidy 1 = RES_REWARD_GENESIS /\
  issuance_epoch 1 = 0.
Proof. repeat split; reflexivity. Qed.

Theorem golden_height_210000_oru_epoch :
  oru_subsidy 210000 = 50000000000 /\
  issuance_epoch 210000 = 1.
Proof. split; reflexivity. Qed.

Theorem golden_height_210000_res :
  res_subsidy 210000 = 14142196.
Proof. vm_compute. reflexivity. Qed.

Theorem golden_height_420000_oru_epoch :
  oru_subsidy 420000 = 25000000000 /\
  issuance_epoch 420000 = 2.
Proof. split; reflexivity. Qed.

Theorem golden_height_420000_res :
  res_subsidy 420000 = 7698004.
Proof. vm_compute. reflexivity. Qed.

Definition cpp_refinement_proved : bool := false.

Theorem issuance_boundary_formula_is_height_div_interval :
  forall h, 0 <= h -> issuance_epoch h = h / 210000.
Proof.
  intros h Hh.
  unfold issuance_epoch, HALVING_INTERVAL.
  destruct (Z.ltb_spec h 0); [lia | reflexivity].
Qed.
