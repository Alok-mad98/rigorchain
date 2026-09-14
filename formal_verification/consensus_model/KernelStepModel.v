(** Model kernel_step on KernelInputV2. Not UnifiedTransition.kernel_step.
    Not ConnectBlock. Not extraction.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Bool.Bool.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import KernelInputV2.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.

Record ModelState := {
  ms_utxo : UTXOState;
  ms_utxo_wf : bool
}.

Definition kernel_step (s : ModelState) (k : KernelInputV2) : ModelState :=
  if validation_all_ok (kiv2_result k)
  then
    {| ms_utxo := apply_block (kiv2_block k) (ms_utxo s);
       ms_utxo_wf := ms_utxo_wf s |}
  else s.

Theorem accepted_step_preserves_utxo_wf :
  forall (s : ModelState) (k : KernelInputV2),
    accepted_validation (kiv2_result k) ->
    ms_utxo_wf s = true ->
    ms_utxo_wf (kernel_step s k) = true.
Proof.
  intros s k Hacc Hwf.
  unfold kernel_step, accepted_validation in *.
  rewrite Hacc.
  simpl.
  exact Hwf.
Qed.

Theorem rejected_step_is_identity :
  forall (s : ModelState) (k : KernelInputV2),
    validation_all_ok (kiv2_result k) = false ->
    kernel_step s k = s.
Proof.
  intros s k Hrej.
  unfold kernel_step.
  rewrite Hrej.
  reflexivity.
Qed.

Theorem deterministic_kernel_transition :
  forall (s : ModelState) (k : KernelInputV2) (s1 s2 : ModelState),
    kernel_step s k = s1 ->
    kernel_step s k = s2 ->
    s1 = s2.
Proof.
  intros s k s1 s2 H1 H2.
  rewrite <- H1. rewrite <- H2.
  reflexivity.
Qed.
