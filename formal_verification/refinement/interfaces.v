(** ConsensusRefinement.interfaces

    FUTURE_REFINEMENT_BOUNDARY

    Refinement *interface* only. Bodies are Parameter / theorem
    statements are Admitted. Not ConnectBlock. Not extraction.
    Not an internal model consistency proof (see consensus_model/*Properties.v).

    decode_block :
      BlockBytes -> option Block

    validate_block :
      Block -> UTXOState -> ValidationResult

    make_kernel_input :
      ValidationResult -> KernelInput

    KernelInput here is the *shape view* [ExistingKernelInput].
    UnifiedTransition.Record KernelInput is unchanged.
    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import KernelInputV2.

Definition KernelInput := ExistingKernelInput.

Parameter decode_block : DecodeModel.
Parameter validate_block : Block -> UTXOState -> ValidationResult.
Parameter make_kernel_input : ValidationResult -> KernelInput.

Definition kernel_input_of_validation : ValidationResult -> KernelInput :=
  make_kernel_input.

Theorem decode_block_deterministic :
  forall (bs : BlockBytes) (b1 b2 : Block),
    decode_block bs = Some b1 ->
    decode_block bs = Some b2 ->
    b1 = b2.
Proof.
Admitted.

Definition input_found (u : UTXOState) (i : TxInput) : Prop :=
  lookup u (previous_outpoint i) <> None.

Theorem validate_inputs_sound :
  forall (b : Block) (u : UTXOState),
    inputs_valid (validate_block b u) = true ->
    List.Forall
      (fun tx => List.Forall (input_found u) (inputs tx))
      (transactions b).
Proof.
Admitted.

Theorem utxo_lookup_correct :
  forall (s : UTXOState) (k : OutPoint) (v : TxOutput),
    lookup (insert s k v) k = Some v.
Proof.
Admitted.

Theorem kernel_input_complete :
  forall (r : ValidationResult),
    ki_utxo_ok (make_kernel_input r) = inputs_valid r /\
    ki_script_ok (make_kernel_input r) = script_valid r.
Proof.
Admitted.
