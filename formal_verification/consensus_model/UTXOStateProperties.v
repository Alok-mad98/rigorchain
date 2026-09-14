(** Internal UTXO map lemmas. Not C++. Not KernelInputCandidate.
    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import UTXOState.

Theorem insert_lookup :
  forall (s : UTXOState) (k : OutPoint) (v : TxOutput),
    lookup (insert s k v) k = Some v.
Proof.
  intros s k v.
  unfold insert.
  simpl.
  rewrite beq_outpoint_refl.
  reflexivity.
Qed.

Theorem remove_lookup :
  forall (s : UTXOState) (k : OutPoint),
    lookup (remove s k) k = None.
Proof.
  intros s k.
  induction s as [|[k' v] rest IH]; simpl.
  - reflexivity.
  - destruct (beq_outpoint k k') eqn:Hk.
    + apply IH.
    + simpl. rewrite Hk. apply IH.
Qed.

Lemma remove_idempotent :
  forall (s : UTXOState) (k : OutPoint),
    remove (remove s k) k = remove s k.
Proof.
  intros s k.
  induction s as [|[k' v] rest IH]; simpl.
  - reflexivity.
  - destruct (beq_outpoint k k') eqn:Hk.
    + apply IH.
    + simpl. rewrite Hk. rewrite IH. reflexivity.
Qed.

Theorem update_remove_consistency :
  forall (s : UTXOState) (k : OutPoint) (v : TxOutput),
    remove (insert s k v) k = remove s k.
Proof.
  intros s k v.
  unfold insert.
  simpl.
  rewrite beq_outpoint_refl.
  apply remove_idempotent.
Qed.

Theorem insert_then_lookup :
  forall (s : UTXOState) (k : OutPoint) (v : TxOutput),
    lookup (insert s k v) k = Some v.
Proof.
  apply insert_lookup.
Qed.

Theorem remove_then_lookup_none :
  forall (s : UTXOState) (k : OutPoint),
    lookup (remove s k) k = None.
Proof.
  apply remove_lookup.
Qed.

Lemma lookup_remove_other :
  forall (s : UTXOState) (k1 k2 : OutPoint),
    beq_outpoint k1 k2 = false ->
    lookup (remove s k1) k2 = lookup s k2.
Proof.
  intros s k1 k2 Hneq.
  induction s as [|[k' v] rest IH]; simpl.
  - reflexivity.
  - destruct (beq_outpoint k1 k') eqn:Hk1.
    + destruct (beq_outpoint k2 k') eqn:Hk2.
      * apply beq_outpoint_true in Hk1.
        apply beq_outpoint_true in Hk2.
        subst. rewrite beq_outpoint_refl in Hneq. discriminate.
      * apply IH.
    + simpl.
      destruct (beq_outpoint k2 k') eqn:Hk2.
      * reflexivity.
      * apply IH.
Qed.

Theorem different_outpoint_independence :
  forall (s : UTXOState) (k1 k2 : OutPoint) (v : TxOutput),
    beq_outpoint k1 k2 = false ->
    lookup (insert s k1 v) k2 = lookup s k2 /\
    lookup (remove s k1) k2 = lookup s k2.
Proof.
  intros s k1 k2 v Hneq.
  split.
  - unfold insert. simpl.
    rewrite beq_outpoint_sym in Hneq.
    rewrite Hneq.
    rewrite beq_outpoint_sym in Hneq.
    apply lookup_remove_other. exact Hneq.
  - apply lookup_remove_other. exact Hneq.
Qed.

Theorem spend_requires_existing_output :
  forall (s : UTXOState) (k : OutPoint) (s' : UTXOState),
    spend s k = Some s' ->
    lookup s k <> None.
Proof.
  intros s k s' Hsp.
  unfold spend in Hsp.
  destruct (lookup s k) eqn:Hlk.
  - discriminate.
  - discriminate.
Qed.

Theorem cannot_spend_removed_output :
  forall (s : UTXOState) (k : OutPoint),
    spend (remove s k) k = None.
Proof.
  intros s k.
  unfold spend.
  rewrite remove_lookup.
  reflexivity.
Qed.
