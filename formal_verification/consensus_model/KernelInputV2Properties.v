(** Internal KernelInputV2 field consistency. Not C++ marshalling.
    Not trace -> KernelInput. Not UnifiedTransition.KernelInput.

    Model [make_kernel_input] builds ExistingKernelInput from a
    KernelInputV2. The refinement Parameter of the same name in
    interfaces.v is FUTURE_REFINEMENT_BOUNDARY.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import ZArith.ZArith.
From ConsensusModel Require Import KernelInputV2.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationProperties.

Local Open Scope Z_scope.

Definition make_kernel_input (k : KernelInputV2) : ExistingKernelInput :=
  {| ki_utxo_ok := inputs_valid (kiv2_result k);
     ki_incoming := 0;
     ki_outgoing := 0;
     ki_fees := 0;
     ki_burn := 0;
     ki_subsidy := 0;
     ki_res_mint := 0;
     ki_script_ok := script_valid (kiv2_result k);
     ki_pow_ok := kiv2_pow_ok k;
     ki_asert_ok := kiv2_asert_ok k |}.

Theorem make_kernel_input_pow_source :
  forall (k : KernelInputV2),
    ki_pow_ok (make_kernel_input k) = kiv2_pow_ok k.
Proof.
  intros k. reflexivity.
Qed.

Theorem make_kernel_input_asert_source :
  forall (k : KernelInputV2),
    ki_asert_ok (make_kernel_input k) = kiv2_asert_ok k.
Proof.
  intros k. reflexivity.
Qed.

Theorem make_kernel_input_field_consistency :
  forall (k : KernelInputV2),
    let i := make_kernel_input k in
    ki_pow_ok i = kiv2_pow_ok k /\
    ki_asert_ok i = kiv2_asert_ok k /\
    ki_utxo_ok i = inputs_valid (kiv2_result k) /\
    ki_script_ok i = script_valid (kiv2_result k).
Proof.
  intros k.
  repeat split; reflexivity.
Qed.

Theorem make_kernel_input_sound :
  forall (k : KernelInputV2),
    ConsensusModel.ValidationResult.accepted_validation (kiv2_result k) ->
    ki_utxo_ok (make_kernel_input k) = true /\
    ki_script_ok (make_kernel_input k) = true /\
    ki_utxo_ok (make_kernel_input k) = inputs_valid (kiv2_result k) /\
    ki_script_ok (make_kernel_input k) = script_valid (kiv2_result k) /\
    ki_pow_ok (make_kernel_input k) = kiv2_pow_ok k /\
    ki_asert_ok (make_kernel_input k) = kiv2_asert_ok k.
Proof.
  intros k Hacc.
  unfold ConsensusModel.ValidationResult.accepted_validation in Hacc.
  pose proof (ConsensusModel.ValidationProperties.accepted_validation
                (kiv2_result k) Hacc) as Hv.
  destruct Hv as [Hin [Hsc _]].
  unfold make_kernel_input. simpl.
  rewrite Hin, Hsc.
  repeat split; reflexivity.
Qed.
