(** ConsensusModel.ReorgSafetyModel

    Chain-selection safety on Fork / CommonAncestor / ChainWeight.
    Builds on ReorgModel. Not ActivateBestChain. Not ConnectBlock.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import ValidationModel.
From ConsensusModel Require Import ChainState.
From ConsensusModel Require Import ReorgModel.
Import ListNotations.

Definition CommonAncestor := bytes32.

Fixpoint chain_wf_full (s : ChainState) : bool :=
  match s with
  | nil => true
  | fr :: rest =>
      andb (Nat.eqb (fr_height fr) (List.length s)) (chain_wf_full rest)
  end.

Lemma wf_full_height :
  forall s, chain_wf_full s = true -> chain_height s = List.length s.
Proof.
  intros s Hwf.
  destruct s as [|fr rest]; simpl.
  - reflexivity.
  - simpl in Hwf.
    apply andb_true_iff in Hwf.
    destruct Hwf as [Hh _].
    apply Nat.eqb_eq in Hh.
    exact Hh.
Qed.

Lemma connect_block_wf_full :
  forall s b s',
    chain_wf_full s = true ->
    connect_block_model s b = Some s' ->
    chain_wf_full s' = true.
Proof.
  intros s b s' Hwf Hc.
  unfold connect_block_model in Hc.
  destruct (validation_all_ok (validate_block b (chain_utxo s))) eqn:Hv;
    [| discriminate].
  injection Hc as Hs. subst s'.
  simpl.
  apply andb_true_iff.
  split.
  - apply Nat.eqb_eq.
    unfold make_frame. simpl.
    rewrite (wf_full_height s Hwf).
    reflexivity.
  - exact Hwf.
Qed.

Lemma disconnect_block_wf_full :
  forall s b s',
    chain_wf_full s = true ->
    disconnect_block_model s b = Some s' ->
    chain_wf_full s' = true.
Proof.
  intros s b s' Hwf Hd.
  unfold disconnect_block_model in Hd.
  destruct s as [|fr rest]; [discriminate|].
  destruct (beq_bytes (fr_tip fr) (block_id b)) eqn:He; [|discriminate].
  injection Hd as Hs. subst s'.
  simpl in Hwf.
  apply andb_true_iff in Hwf.
  destruct Hwf as [_ Hrest].
  exact Hrest.
Qed.

Lemma disconnect_blocks_wf_full :
  forall bs s s',
    chain_wf_full s = true ->
    disconnect_blocks s bs = Some s' ->
    chain_wf_full s' = true.
Proof.
  induction bs as [|b rest IH]; intros s s' Hwf Hd.
  - simpl in Hd. injection Hd as Hs. subst s'. exact Hwf.
  - simpl in Hd.
    destruct (disconnect_block_model s b) as [s1|] eqn:Hd1; [|discriminate].
    apply IH with (s := s1).
    + apply disconnect_block_wf_full with (s := s) (b := b); assumption.
    + exact Hd.
Qed.

Lemma connect_blocks_wf_full :
  forall bs s s',
    chain_wf_full s = true ->
    connect_blocks s bs = Some s' ->
    chain_wf_full s' = true.
Proof.
  induction bs as [|b rest IH]; intros s s' Hwf Hc.
  - simpl in Hc. injection Hc as Hs. subst s'. exact Hwf.
  - simpl in Hc.
    destruct (connect_block_model s b) as [s1|] eqn:Hc1; [|discriminate].
    apply IH with (s := s1).
    + apply connect_block_wf_full with (s := s) (b := b); assumption.
    + exact Hc.
Qed.

Lemma connect_blocks_fail :
  forall s bs,
    connect_blocks_ok s bs = false ->
    connect_blocks s bs = None.
Proof.
  intros s bs. revert s.
  induction bs as [|b rest IH]; intros s Hok.
  - simpl in Hok. discriminate.
  - simpl in Hok. simpl.
    destruct (connect_block_model s b) as [s1|] eqn:Hc.
    + apply IH. exact Hok.
    + reflexivity.
Qed.

Definition select_heavier (a b : Fork) : Fork :=
  if Nat.ltb (ChainWeight a) (ChainWeight b) then b else a.

Theorem common_ancestor_preserved :
  forall (a b : Fork),
    competing_forks a b ->
    exists anc : CommonAncestor,
      common_ancestor a b = Some anc /\
      anc = fk_ancestor a /\
      anc = fk_ancestor b.
Proof.
  intros a b Hcomp.
  unfold competing_forks in Hcomp.
  apply beq_bytes_true in Hcomp.
  destruct a as [ancA blA], b as [ancB blB].
  simpl in Hcomp. subst ancB.
  exists ancA.
  unfold common_ancestor. simpl.
  rewrite beq_bytes_refl.
  split; [reflexivity|].
  split; reflexivity.
Qed.

Theorem valid_reorg_preserves_state :
  forall (s : ChainState) (old new : Fork) (s' : ChainState),
    chain_wf_full s = true ->
    reorg_model s old new = Some s' ->
    chain_wf_full s' = true.
Proof.
  intros s old new s' Hwf Hr.
  destruct (reorg_disconnect_connect_consistent s old new s' Hr)
    as [s0 [Hd Hc]].
  apply connect_blocks_wf_full with (bs := fk_blocks new) (s := s0).
  - apply disconnect_blocks_wf_full with (bs := rev (fk_blocks old)) (s := s);
      assumption.
  - exact Hc.
Qed.

Theorem heavier_chain_selected :
  forall (a b : Fork),
    (ChainWeight a < ChainWeight b -> select_heavier a b = b) /\
    (forall (s s' : ChainState),
       reorg_model s a b = Some s' -> ChainWeight a < ChainWeight b).
Proof.
  intros a b.
  split.
  - intros Hlt.
    unfold select_heavier.
    apply Nat.ltb_lt in Hlt.
    rewrite Hlt.
    reflexivity.
  - intros s s' Hr.
    unfold reorg_model in Hr.
    destruct (beq_bytes (fk_ancestor a) (fk_ancestor b)); [|discriminate].
    destruct (Nat.ltb (ChainWeight a) (ChainWeight b)) eqn:Hwt; [|discriminate].
    apply Nat.ltb_lt in Hwt.
    exact Hwt.
Qed.

Theorem invalid_branch_cannot_replace :
  forall (s : ChainState) (old new : Fork) (s0 : ChainState),
    competing_forks old new ->
    Nat.ltb (ChainWeight old) (ChainWeight new) = true ->
    disconnect_blocks s (rev (fk_blocks old)) = Some s0 ->
    connect_blocks_ok s0 (fk_blocks new) = false ->
    reorg_model s old new = None.
Proof.
  intros s old new s0 Hcomp Hwt Hd Hfail.
  unfold reorg_model, competing_forks in *.
  rewrite Hcomp.
  rewrite Hwt.
  rewrite Hd.
  apply connect_blocks_fail.
  exact Hfail.
Qed.
