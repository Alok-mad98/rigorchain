(** ConsensusModel.ReorgModel

    Chain *selection* model: short fork -> longer fork.
    Distinct from block validation ([validate_block] / connect None).

    Not C++ ActivateBestChain. Not ConnectBlock. Not extraction.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import ChainState.
Import ListNotations.

Record Fork := {
  fk_ancestor : bytes32;
  fk_blocks : list Block
}.

Definition ForkA := Fork.
Definition ForkB := Fork.

Definition ChainWeight (f : Fork) : nat :=
  List.length (fk_blocks f).

Definition competing_forks (a b : Fork) : Prop :=
  beq_bytes (fk_ancestor a) (fk_ancestor b) = true.

Definition common_ancestor (a b : Fork) : option bytes32 :=
  if beq_bytes (fk_ancestor a) (fk_ancestor b)
  then Some (fk_ancestor a)
  else None.

Fixpoint disconnect_blocks (s : ChainState) (bs : list Block) : option ChainState :=
  match bs with
  | nil => Some s
  | b :: rest =>
      match disconnect_block_model s b with
      | Some s' => disconnect_blocks s' rest
      | None => None
      end
  end.

Fixpoint connect_blocks (s : ChainState) (bs : list Block) : option ChainState :=
  match bs with
  | nil => Some s
  | b :: rest =>
      match connect_block_model s b with
      | Some s' => connect_blocks s' rest
      | None => None
      end
  end.

Fixpoint connect_blocks_ok (s : ChainState) (bs : list Block) : bool :=
  match bs with
  | nil => true
  | b :: rest =>
      match connect_block_model s b with
      | Some s' => connect_blocks_ok s' rest
      | None => false
      end
  end.

(** [fk_blocks] is ancestor-first. Disconnect newest first ([rev]). *)
Definition reorg_model (s : ChainState) (old new : Fork) : option ChainState :=
  if beq_bytes (fk_ancestor old) (fk_ancestor new)
  then
    if Nat.ltb (ChainWeight old) (ChainWeight new)
    then
      match disconnect_blocks s (rev (fk_blocks old)) with
      | Some s0 => connect_blocks s0 (fk_blocks new)
      | None => None
      end
    else None
  else None.

Lemma connect_blocks_some_ok :
  forall (s : ChainState) (bs : list Block) (s' : ChainState),
    connect_blocks s bs = Some s' ->
    connect_blocks_ok s bs = true.
Proof.
  intros s bs.
  revert s.
  induction bs as [|b rest IH]; intros s s' H.
  - reflexivity.
  - simpl in H.
    destruct (connect_block_model s b) as [s1|] eqn:Hc; [| discriminate].
    simpl.
    rewrite Hc.
    apply IH with (s' := s').
    exact H.
Qed.

Theorem common_ancestor_exists :
  forall (a b : Fork),
    competing_forks a b ->
    common_ancestor a b <> None.
Proof.
  intros a b Hcomp.
  unfold competing_forks, common_ancestor in *.
  rewrite Hcomp.
  discriminate.
Qed.

Theorem reorg_disconnect_connect_consistent :
  forall (s : ChainState) (old new : Fork) (s' : ChainState),
    reorg_model s old new = Some s' ->
    exists s0 : ChainState,
      disconnect_blocks s (rev (fk_blocks old)) = Some s0 /\
      connect_blocks s0 (fk_blocks new) = Some s'.
Proof.
  intros s old new s' Hr.
  unfold reorg_model in Hr.
  destruct (beq_bytes (fk_ancestor old) (fk_ancestor new)) eqn:Hanc;
    [| discriminate].
  destruct (Nat.ltb (ChainWeight old) (ChainWeight new)) eqn:Hwt;
    [| discriminate].
  destruct (disconnect_blocks s (rev (fk_blocks old))) as [s0|] eqn:Hd;
    [| discriminate].
  exists s0.
  split.
  - reflexivity.
  - exact Hr.
Qed.

Theorem reorg_preserves_validity :
  forall (s : ChainState) (old new : Fork) (s' : ChainState),
    reorg_model s old new = Some s' ->
    exists s0 : ChainState,
      disconnect_blocks s (rev (fk_blocks old)) = Some s0 /\
      connect_blocks_ok s0 (fk_blocks new) = true.
Proof.
  intros s old new s' Hr.
  destruct (reorg_disconnect_connect_consistent s old new s' Hr)
    as [s0 [Hd Hc]].
  exists s0.
  split.
  - exact Hd.
  - apply connect_blocks_some_ok with (s' := s').
    exact Hc.
Qed.
