(** ConsensusModel.ChainState

    Chain-level evolution. Not C++ ConnectBlock / EvaluateConnect.
    Not extraction. Not Record KernelInput.

    Block validation state: [validate_block] / [connect_block_model]
    returning None.

    Chain selection state: tip / height / weight (see ReorgModel.v).
    Those are distinct. This file does not choose the best chain.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.
Import ListNotations.

Record ChainMeta := {
  cm_blocks : nat;
  cm_txs : nat
}.

Record ChainFrame := {
  fr_height : nat;
  fr_tip : bytes32;
  fr_utxo : UTXOState;
  fr_meta : ChainMeta
}.

(** Tip-first stack. Head is the current tip frame. *)
Definition ChainState := list ChainFrame.

Definition empty_meta : ChainMeta :=
  {| cm_blocks := 0; cm_txs := 0 |}.

Definition chain_height (s : ChainState) : nat :=
  match s with
  | nil => 0
  | fr :: _ => fr_height fr
  end.

Definition chain_utxo (s : ChainState) : UTXOState :=
  match s with
  | nil => nil
  | fr :: _ => fr_utxo fr
  end.

Definition chain_tip (s : ChainState) : option bytes32 :=
  match s with
  | nil => None
  | fr :: _ => Some (fr_tip fr)
  end.

Definition chain_meta (s : ChainState) : ChainMeta :=
  match s with
  | nil => empty_meta
  | fr :: _ => fr_meta fr
  end.

(** Model block identity (not a C++ hash). *)
Definition block_id (b : Block) : bytes32 :=
  bh_merkle_root (header b).

Definition make_frame (s : ChainState) (b : Block) : ChainFrame :=
  {| fr_height := S (chain_height s);
     fr_tip := block_id b;
     fr_utxo := apply_block b (chain_utxo s);
     fr_meta :=
       {| cm_blocks := S (cm_blocks (chain_meta s));
          cm_txs := cm_txs (chain_meta s) + List.length (transactions b) |}
  |}.

(** Height equals stack length. *)
Definition chain_wf (s : ChainState) : bool :=
  match s with
  | nil => true
  | fr :: _ => Nat.eqb (fr_height fr) (List.length s)
  end.

Definition connect_block_model (s : ChainState) (b : Block) : option ChainState :=
  if validation_all_ok (validate_block b (chain_utxo s))
  then Some (make_frame s b :: s)
  else None.

Definition disconnect_block_model (s : ChainState) (b : Block) : option ChainState :=
  match s with
  | nil => None
  | fr :: rest =>
      if beq_bytes (fr_tip fr) (block_id b)
      then Some rest
      else None
  end.

Definition apply_connect (s : ChainState) (b : Block) : ChainState :=
  match connect_block_model s b with
  | Some s' => s'
  | None => s
  end.

Lemma chain_height_length :
  forall (s : ChainState),
    chain_wf s = true ->
    chain_height s = List.length s.
Proof.
  intros s Hwf.
  destruct s as [|fr rest]; simpl.
  - reflexivity.
  - simpl in Hwf.
    apply Nat.eqb_eq in Hwf.
    exact Hwf.
Qed.

Theorem connect_then_disconnect_inverse :
  forall (s : ChainState) (b : Block) (s' : ChainState),
    connect_block_model s b = Some s' ->
    disconnect_block_model s' b = Some s.
Proof.
  intros s b s' Hc.
  unfold connect_block_model in Hc.
  destruct (validation_all_ok (validate_block b (chain_utxo s))) eqn:Hv;
    [| discriminate].
  injection Hc as Hs.
  subst s'.
  unfold disconnect_block_model, make_frame.
  simpl.
  rewrite beq_bytes_refl.
  reflexivity.
Qed.

Theorem disconnect_then_connect_inverse :
  forall (s : ChainState) (b : Block) (s' : ChainState),
    connect_block_model s b = Some s' ->
    match disconnect_block_model s' b with
    | Some s0 => connect_block_model s0 b
    | None => None
    end = Some s'.
Proof.
  intros s b s' Hc.
  pose proof (connect_then_disconnect_inverse s b s' Hc) as Hd.
  rewrite Hd.
  exact Hc.
Qed.

Theorem accepted_block_preserves_state_wf :
  forall (s : ChainState) (b : Block) (s' : ChainState),
    chain_wf s = true ->
    connect_block_model s b = Some s' ->
    chain_wf s' = true.
Proof.
  intros s b s' Hwf Hc.
  unfold connect_block_model in Hc.
  destruct (validation_all_ok (validate_block b (chain_utxo s))) eqn:Hv;
    [| discriminate].
  injection Hc as Hs.
  subst s'.
  unfold chain_wf, make_frame.
  simpl.
  apply Nat.eqb_eq.
  rewrite chain_height_length by exact Hwf.
  reflexivity.
Qed.

Theorem rejected_block_no_state_change :
  forall (s : ChainState) (b : Block),
    validation_all_ok (validate_block b (chain_utxo s)) = false ->
    apply_connect s b = s.
Proof.
  intros s b Hrej.
  unfold apply_connect, connect_block_model.
  rewrite Hrej.
  reflexivity.
Qed.

Theorem deterministic_chain_transition :
  forall (s : ChainState) (b : Block)
         (r1 r2 : option ChainState),
    connect_block_model s b = r1 ->
    connect_block_model s b = r2 ->
    r1 = r2.
Proof.
  intros s b r1 r2 H1 H2.
  rewrite <- H1. rewrite <- H2.
  reflexivity.
Qed.
