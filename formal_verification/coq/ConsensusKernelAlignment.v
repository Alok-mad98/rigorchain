(** ConsensusKernelAlignment.v — P049 C++ Kernel ↔ Coq canonical mapping.

    Re-exports existing objects. Does not redefine epoch, ORU, RES, draw,
    classify, or supply. Not a refinement. Not an equivalence proof.

      refinement_proved = false
      equivalence_proved = false

    No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Lia.
Import ListNotations.
From Rigor Require Import ResSchedule.
Require Import Rigor.legacy_account_model.Selection.
Require Import Rigor.Kernel.KernelSelection.
From Rigor.FeeBurn Require Import Accounting.
From RigorV01 Require Import IssuanceHeightBoundary.
From RigorV01 Require Import CanonicalIssuance.
From RigorV01 Require Import GenesisZeroIssuance.
From RigorV01 Require Import MintSupplyConservation.
From RigorV01 Require Import WindowGeneric.
From RigorV01 Require Import ResMintCorrectness.

Local Open Scope Z_scope.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem p049_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem p049_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

(** * Epoch — C++ [RigorIssuanceEpoch] / [GetResMintReward] / [GetBlockSubsidy] *)

Theorem kernel_epoch_formula :
  forall h, 0 <= h -> IssuanceHeightBoundary.issuance_epoch h = h / 210000.
Proof. apply IssuanceHeightBoundary.issuance_boundary_formula_is_height_div_interval. Qed.

Theorem kernel_epoch_pins :
  IssuanceHeightBoundary.issuance_epoch 0 = 0 /\
  IssuanceHeightBoundary.issuance_epoch 1 = 0 /\
  IssuanceHeightBoundary.issuance_epoch 209999 = 0 /\
  IssuanceHeightBoundary.issuance_epoch 210000 = 1 /\
  IssuanceHeightBoundary.issuance_epoch 210001 = 1.
Proof. repeat split; reflexivity. Qed.

Theorem kernel_oru_res_same_epoch :
  forall h,
    IssuanceHeightBoundary.issuance_epoch_oru h =
    IssuanceHeightBoundary.issuance_epoch_res h.
Proof. apply IssuanceHeightBoundary.oru_res_halving_boundary_agree. Qed.

(** * ORU issuance — C++ [GetBlockSubsidy] *)

Theorem kernel_oru_genesis_zero :
  IssuanceHeightBoundary.oru_subsidy 0 = 0.
Proof. exact IssuanceHeightBoundary.genesis_reward_zero. Qed.

Theorem kernel_oru_height_1 :
  IssuanceHeightBoundary.oru_subsidy 1 = 100000000000.
Proof. reflexivity. Qed.

Theorem kernel_oru_halving_boundary :
  IssuanceHeightBoundary.oru_subsidy 209999 = 100000000000 /\
  IssuanceHeightBoundary.oru_subsidy 210000 = 50000000000.
Proof. split; reflexivity. Qed.

Theorem kernel_oru_cumulative_coins :
  CanonicalIssuance.total_oru_issued 209999 / CanonicalIssuance.ORU_COIN = 209999000 /\
  CanonicalIssuance.total_oru_issued 210000 / CanonicalIssuance.ORU_COIN = 209999500.
Proof.
  split.
  - exact CanonicalIssuance.first_epoch_oru_coins.
  - exact CanonicalIssuance.through_first_halving_oru_coins.
Qed.

Theorem kernel_no_height_1_compensation :
  IssuanceHeightBoundary.oru_subsidy 1 <>
    IssuanceHeightBoundary.ORU_INITIAL_SATS + IssuanceHeightBoundary.ORU_INITIAL_SATS.
Proof. exact IssuanceHeightBoundary.no_height_1_compensation. Qed.

(** * RES schedule vs actual issuance

    C++ [GetResMintReward(0)] is the schedule value W(0).
    ConnectBlock genesis skip / [res_issued_at_height 0] is actual issuance 0.
    These are different facts. *)

Theorem kernel_res_schedule_at_genesis :
  get_res_mint_reward 0 = RES_REWARD_GENESIS.
Proof. exact reward_at_height_zero. Qed.

Theorem kernel_res_actual_issuance_at_genesis :
  CanonicalIssuance.res_issued_at_height 0 = 0.
Proof. exact CanonicalIssuance.res_issued_zero. Qed.

Theorem kernel_genesis_schedule_is_not_actual_issuance :
  get_res_mint_reward 0 = RES_REWARD_GENESIS /\
  CanonicalIssuance.res_issued_at_height 0 = 0 /\
  get_res_mint_reward 0 <> CanonicalIssuance.res_issued_at_height 0.
Proof.
  split; [exact reward_at_height_zero|].
  split; [exact CanonicalIssuance.res_issued_zero|].
  rewrite reward_at_height_zero.
  rewrite CanonicalIssuance.res_issued_zero.
  unfold RES_REWARD_GENESIS, RES_COIN.
  discriminate.
Qed.

Theorem kernel_genesis_connectblock_supply_zero :
  us_ledger_oru genesis_supply = 0 /\
  us_circulating_oru genesis_supply = 0 /\
  us_res genesis_supply = 0.
Proof.
  destruct GenesisZeroIssuance.genesis_supply_zero as [Ho [Hc [Hr _]]].
  repeat split; assumption.
Qed.

(** * Maturity — not issuance, not epoch, not draw eligibility *)

Theorem kernel_oru_maturity_100 :
  WindowGeneric.COINBASE_MATURITY = 100.
Proof. reflexivity. Qed.

Theorem kernel_res_maturity_100 :
  ResMintCorrectness.res_mint_maturity_blocks = 100 /\
  ResMintCorrectness.res_mint_maturity_blocks = ResMintCorrectness.coinbase_maturity.
Proof. split; reflexivity. Qed.

Theorem kernel_height_1_immature_at_100 :
  WindowGeneric.coinbase_mature 100 1 = false.
Proof. reflexivity. Qed.

Theorem kernel_height_1_mature_at_101 :
  WindowGeneric.coinbase_mature 101 1 = true.
Proof. reflexivity. Qed.

(** * Draw — C++ [ResolveConvergenceDraw] / Coq [select_winner] *)

Theorem kernel_empty_F_has_no_winner :
  forall C B h, select_winner [] C B h = None.
Proof. intros. reflexivity. Qed.

Theorem kernel_winner_in_F :
  forall l C B h w,
    select_winner l C B h = Some w -> List.In w l.
Proof. exact KernelSelection.select_winner_in. Qed.

Definition kernel_tie_breaks_to_smaller_txid :=
  KernelSelection.pick_better_tx_tie_txid.

(** * Supply claimable cap — C++ [RigorSupplyDelta] oru_claimable *)

Theorem kernel_oru_claimable_formula :
  forall fees burn subsidy,
    coinbase_oru_cap fees burn subsidy = (fees - burn) + subsidy.
Proof. intros. reflexivity. Qed.

(** * Classification honesty: this file does not introduce a second matcher. *)
