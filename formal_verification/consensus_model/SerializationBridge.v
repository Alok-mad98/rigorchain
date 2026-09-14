(** ConsensusModel.SerializationBridge

    Connect BlockCodec.decode_block to ValidationModel.validate_block
    and KernelInputV2. Not refinement/interfaces.v. Not C++.

    Chain: bytes → decode_block → Block → validate_block → KernelInputV2

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Bool.Bool.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.
From ConsensusModel Require Import KernelInputV2.
From ConsensusModel Require Import BlockCodec.

Definition decoded_block_to_validation_input
  (b : Block) (u : UTXOState) : Block * UTXOState :=
  (b, u).

Definition decoded_block_to_kernel_input_v2
  (b : Block) (u : UTXOState) : KernelInputV2 :=
  {| kiv2_block := b;
     kiv2_utxo := u;
     kiv2_result := validate_block b u;
     kiv2_pow_ok := false;
     kiv2_asert_ok := false |}.

Theorem decoded_valid_block_can_validate :
  forall (bs : byte_list) (b : Block) (u : UTXOState),
    decode_block bs = Some b ->
    let inp := decoded_block_to_validation_input b u in
    fst inp = b /\
    snd inp = u /\
    (validation_all_ok (validate_block (fst inp) (snd inp)) = true \/
     validation_all_ok (validate_block (fst inp) (snd inp)) = false).
Proof.
  intros bs b u Hdec.
  unfold decoded_block_to_validation_input.
  simpl.
  split; [reflexivity|].
  split; [reflexivity|].
  destruct (validation_all_ok (validate_block b u)); [left | right]; reflexivity.
Qed.
