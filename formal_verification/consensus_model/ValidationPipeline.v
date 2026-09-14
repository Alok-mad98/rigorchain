(** ConsensusModel.ValidationPipeline

    Model-internal validation closure:
      bytes → decode_block → Block → validate_block → ValidationResult
           → KernelInputV2 → kernel_step → ChainState

    Connects TransactionCodec, BlockCodec, ValidationModel, UTXOState.
    Not ConnectBlock. Not EvaluateConnect. Not extraction.
    Not refinement/interfaces.v.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Bool.Bool.
From ConsensusModel Require Import ByteModel.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.
From ConsensusModel Require Import TransactionCodec.
From ConsensusModel Require Import BlockCodec.
From ConsensusModel Require Import KernelInputV2.
From ConsensusModel Require Import KernelInputConstruction.
From ConsensusModel Require Import KernelStepModel.
From ConsensusModel Require Import ChainState.

Definition validate_decoded_block
  (b : Block) (u : UTXOState) : option ValidationResult :=
  Some (validate_block b u).

Definition validate_decoded_block_bytes
  (bs : byte_list) (u : UTXOState) : option ValidationResult :=
  match decode_block bs with
  | None => None
  | Some b => validate_decoded_block b u
  end.

(** ChainState kernel step: failed validation is identity.
    Accepted validation uses [apply_connect] (model connect, not C++). *)
Definition chain_kernel_step (s : ChainState) (k : KernelInputV2) : ChainState :=
  if validation_all_ok (kiv2_result k)
  then apply_connect s (kiv2_block k)
  else s.

Definition run_validation_pipeline
  (bs : byte_list) (s : ChainState) (pow_ok asert_ok : bool)
  : option (ValidationResult * KernelInputV2 * ChainState) :=
  match decode_block bs with
  | None => None
  | Some b =>
      match validate_decoded_block b (chain_utxo s) with
      | None => None
      | Some r =>
          let k := make_kernel_input_v2 b (chain_utxo s) r pow_ok asert_ok in
          Some (r, k, chain_kernel_step s k)
      end
  end.

Theorem decoded_block_validation_deterministic :
  forall (b : Block) (u : UTXOState)
         (r1 r2 : ValidationResult),
    validate_decoded_block b u = Some r1 ->
    validate_decoded_block b u = Some r2 ->
    r1 = r2.
Proof.
  intros b u r1 r2 H1 H2.
  unfold validate_decoded_block in H1, H2.
  injection H1 as H1.
  injection H2 as H2.
  rewrite <- H1. rewrite <- H2.
  reflexivity.
Qed.

Theorem validation_failure_no_transition :
  forall (s : ChainState) (ms : ModelState) (k : KernelInputV2),
    validation_all_ok (kiv2_result k) = false ->
    chain_kernel_step s k = s /\
    kernel_step ms k = ms.
Proof.
  intros s ms k Hfail.
  split.
  - unfold chain_kernel_step. rewrite Hfail. reflexivity.
  - unfold kernel_step. rewrite Hfail. reflexivity.
Qed.

Theorem kernel_input_then_step_deterministic :
  forall (s : ChainState) (b : Block) (u : UTXOState)
         (pow_ok asert_ok : bool)
         (s1 s2 : ChainState)
         (ms : ModelState) (ms1 ms2 : ModelState),
    chain_kernel_step s
      (make_kernel_input_v2 b u (validate_block b u) pow_ok asert_ok) = s1 ->
    chain_kernel_step s
      (make_kernel_input_v2 b u (validate_block b u) pow_ok asert_ok) = s2 ->
    kernel_step ms
      (make_kernel_input_v2 b u (validate_block b u) pow_ok asert_ok) = ms1 ->
    kernel_step ms
      (make_kernel_input_v2 b u (validate_block b u) pow_ok asert_ok) = ms2 ->
    s1 = s2 /\ ms1 = ms2.
Proof.
  intros s b u pow_ok asert_ok s1 s2 ms ms1 ms2 Hs1 Hs2 Hm1 Hm2.
  split.
  - rewrite <- Hs1. rewrite <- Hs2. reflexivity.
  - rewrite <- Hm1. rewrite <- Hm2. reflexivity.
Qed.

Theorem serialization_validation_composition :
  forall (b : Block) (u : UTXOState),
    block_ok b ->
    validate_decoded_block_bytes (encode_block b) u =
    validate_decoded_block b u.
Proof.
  intros b u Hok.
  unfold validate_decoded_block_bytes.
  rewrite (decode_encode_block b Hok).
  reflexivity.
Qed.
