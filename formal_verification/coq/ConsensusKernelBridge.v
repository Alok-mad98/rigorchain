(** ConsensusKernelBridge.v — P052 issuance / epoch / maturity mapping.

    Names the Coq objects that the C++ Kernel APIs correspond to.
    Does not redefine those objects. Does not prove C++ = Coq.

      refinement_proved = false
      equivalence_proved = false

    No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import ZArith.ZArith.
From Rigor Require Import ResSchedule.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import CanonicalIssuance.
From RigorV01 Require Import WindowGeneric.
From RigorV01 Require Import ResMintCorrectness.

Local Open Scope Z_scope.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem p052_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem p052_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

(** C++ [RigorIssuanceEpoch] maps to this object. Not a second epoch. *)
Definition cpp_epoch_bridge : Z -> Z := issuance_epoch.

(** C++ [GetBlockSubsidy] / [ComputeRigorIssuance.oru_subsidy]. *)
Definition cpp_oru_subsidy_bridge : Z -> Z := oru_subsidy.

(** C++ [ResDestroyCostAtHeight] — display destroy_cost(h), including height 0 = P0. *)
Definition cpp_res_schedule_bridge : Z -> Z := res_destroy_cost_at_height.

(** Actual RES issuance model; height 0 is 0. Not the schedule. *)
Definition cpp_res_actual_issuance_bridge : Z -> Z :=
  CanonicalIssuance.res_issued_at_height.

(** C++ [ComputeRigorIssuance] packaging of the three schedules. *)
Definition cpp_compute_issuance_bridge (h : Z) : Z * Z * Z :=
  (issuance_epoch h, oru_subsidy h, res_destroy_cost_at_height h).

Theorem cpp_epoch_bridge_is_canonical :
  cpp_epoch_bridge = issuance_epoch.
Proof. reflexivity. Qed.

Theorem cpp_oru_subsidy_bridge_is_canonical :
  cpp_oru_subsidy_bridge = oru_subsidy.
Proof. reflexivity. Qed.

Theorem cpp_res_schedule_bridge_is_canonical :
  cpp_res_schedule_bridge = res_destroy_cost_at_height.
Proof. reflexivity. Qed.

Theorem cpp_epoch_bridge_formula :
  forall h, 0 <= h -> cpp_epoch_bridge h = h / 210000.
Proof.
  intros h Hh.
  unfold cpp_epoch_bridge.
  apply IssuanceHeightBoundary.issuance_boundary_formula_is_height_div_interval.
  exact Hh.
Qed.

Theorem cpp_epoch_bridge_pins :
  cpp_epoch_bridge 0 = 0 /\
  cpp_epoch_bridge 1 = 0 /\
  cpp_epoch_bridge 209999 = 0 /\
  cpp_epoch_bridge 210000 = 1 /\
  cpp_epoch_bridge 210001 = 1.
Proof. repeat split; reflexivity. Qed.

Theorem cpp_oru_genesis_actual_zero :
  cpp_oru_subsidy_bridge 0 = 0.
Proof. reflexivity. Qed.

Theorem cpp_res_destroy_cost_at_genesis_is_p0 :
  cpp_res_schedule_bridge 0 = RES_DESTROY_COST_GENESIS.
Proof. exact destroy_cost_at_height_zero. Qed.

Theorem cpp_res_actual_at_genesis_is_zero :
  cpp_res_actual_issuance_bridge 0 = 0.
Proof. exact CanonicalIssuance.res_issued_zero. Qed.

Theorem genesis_schedule_value_ne_actual_issuance :
  cpp_res_schedule_bridge 0 <> cpp_res_actual_issuance_bridge 0.
Proof.
  rewrite cpp_res_destroy_cost_at_genesis_is_p0.
  rewrite cpp_res_actual_at_genesis_is_zero.
  unfold RES_DESTROY_COST_GENESIS, RES_COIN.
  discriminate.
Qed.

Theorem cpp_maturity_oru_100 :
  WindowGeneric.COINBASE_MATURITY = 100.
Proof. reflexivity. Qed.

Theorem cpp_maturity_res_100 :
  ResMintCorrectness.res_mint_maturity_blocks = 100.
Proof. reflexivity. Qed.

Theorem cpp_height_1_immature_at_100 :
  WindowGeneric.coinbase_mature 100 1 = false.
Proof. reflexivity. Qed.

Theorem cpp_height_1_mature_at_101 :
  WindowGeneric.coinbase_mature 101 1 = true.
Proof. reflexivity. Qed.
