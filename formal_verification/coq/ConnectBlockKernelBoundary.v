(** ConnectBlockKernelBoundary.v — P057.

    ConnectBlock
            |
            v
    ConsensusKernelInput
            |
            v
    ConsensusKernel
            |
            v
    accept / reject / apply delta

    Proves the boundary: ConnectBlock only prepares inputs and consumes
    the Kernel result. Not a full ConnectBlock proof. Not UTXO / DB / PoW
    / Transaction Kernel.

      ConnectBlockBoundaryRefinement = PASS
      connectblock_full_proof        = false
      refinement_proved              = true   (P056 kernel, unchanged)
      equivalence_proved             = false

    No [Admitted] / [Axiom] / [Parameter] / [Extract Constant]. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.

Require Import Rigor.ConsensusKernelRefinement.
Require Import Rigor.ConnectBlockInputPreparation.
Require Import Rigor.KernelResultApplication.

Local Open Scope Z_scope.

Definition connectblock_full_proof : bool := false.
Definition connectblock_boundary_proved : bool := true.
Definition ConnectBlockBoundaryRefinement : bool := true.

Theorem connectblock_full_not_proved :
  connectblock_full_proof = false.
Proof. reflexivity. Qed.

Theorem connectblock_boundary_flag :
  ConnectBlockBoundaryRefinement = true.
Proof. reflexivity. Qed.

Theorem p056_refinement_still_true :
  ConsensusKernelRefinement.refinement_proved = true.
Proof. reflexivity. Qed.

Theorem p056_equivalence_still_false :
  ConsensusKernelRefinement.equivalence_proved = false.
Proof. reflexivity. Qed.

Definition ConnectBlockOrchestration (s : BlockState) : ConnectBlockAction :=
  ConnectBlockApply (RunKernel (PrepareKernelInput s)).

Definition ConnectBlockCanonical (s : BlockState) : ConnectBlockAction :=
  ConnectBlockApply (RunKernel (KernelInput s)).

Theorem connectblock_kernel_boundary :
  forall s,
    ConnectBlockOrchestration s = ConnectBlockCanonical s.
Proof.
  intros s.
  unfold ConnectBlockOrchestration, ConnectBlockCanonical.
  rewrite prepared_input_eq_canonical.
  reflexivity.
Qed.

Theorem connectblock_only_prepares_and_consumes :
  forall s,
    ConnectBlockOrchestration s =
    ConnectBlockApply (RunKernel (PrepareKernelInput s)).
Proof. intros. reflexivity. Qed.

Theorem input_preparation_alignment :
  forall s, PrepareKernelInput s = KernelInput s.
Proof. exact prepared_input_eq_canonical. Qed.

Theorem result_consumption_is_apply :
  forall r,
    (ckr_ok r = true /\
     ConnectBlockApply r = CB_AcceptApplyDelta (ckr_ledger_delta r)) \/
    (ckr_ok r = false /\ ConnectBlockApply r = CB_Reject).
Proof.
  intros r.
  destruct (ckr_ok r) eqn:E.
  - left. split; [reflexivity|]. apply apply_delta_when_ok. exact E.
  - right. split; [reflexivity|]. apply apply_reject_when_not_ok. exact E.
Qed.

Theorem genesis_orchestration_applies_zero_delta :
  ConnectBlockOrchestration genesis_block_state = CB_AcceptApplyDelta 0.
Proof. vm_compute. reflexivity. Qed.

Definition utxo_refinement_in_scope : bool := false.
Definition db_refinement_in_scope : bool := false.
Definition pow_refinement_in_scope : bool := false.
Definition transaction_kernel_in_scope : bool := false.

Theorem explicit_nongoals :
  utxo_refinement_in_scope = false /\
  db_refinement_in_scope = false /\
  pow_refinement_in_scope = false /\
  transaction_kernel_in_scope = false.
Proof. repeat split; reflexivity. Qed.
