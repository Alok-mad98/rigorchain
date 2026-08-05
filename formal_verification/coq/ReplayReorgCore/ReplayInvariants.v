(** ReplayReorgCore/ReplayInvariants.v — UTXO / rollback replay invariants (PHASE-4C). *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ReorgRules.

(** All stored output values are non-negative (structural supply hygiene). *)
Definition utxo_non_negative (u : UTXOSet) : Prop :=
  forall (op : OutPoint) (out : TxOut),
    In (op, out) u -> (0 <= out_value out)%Z.

Lemma utxo_non_negative_empty : utxo_non_negative empty_utxo.
Proof.
  unfold utxo_non_negative, empty_utxo. intros op out Hin. inversion Hin.
Qed.

Lemma outpoint_eqb_eq :
  forall a b : OutPoint, outpoint_eqb a b = true -> a = b.
Proof.
  intros [ha ia] [hb ib] H.
  simpl in H. apply Bool.andb_true_iff in H as [Hi Hh].
  apply Nat.eqb_eq in Hi. subst ib.
  destruct (hash32_eq_dec ha hb) as [Eh | N]; [|discriminate Hh].
  subst hb. reflexivity.
Qed.

Lemma outpoint_eqb_refl : forall op : OutPoint, outpoint_eqb op op = true.
Proof.
  intros [h i]. simpl. rewrite Nat.eqb_refl. simpl.
  destruct (hash32_eq_dec h h) as [_ | N]; [reflexivity | congruence].
Qed.

Lemma outpoint_eqb_false :
  forall a b : OutPoint, a <> b -> outpoint_eqb a b = false.
Proof.
  intros a b Hne.
  destruct (outpoint_eqb a b) eqn:E; [|reflexivity].
  exfalso. apply Hne. exact (outpoint_eqb_eq a b E).
Qed.

Lemma utxo_lookup_app_none :
  forall (l1 l2 : UTXOSet) (op : OutPoint),
    utxo_lookup l1 op = None ->
    utxo_lookup (l1 ++ l2) op = utxo_lookup l2 op.
Proof.
  induction l1 as [|[op' o'] t IH]; intros l2 op H.
  - reflexivity.
  - simpl in H |- *.
    destruct (outpoint_eqb op op') eqn:E; [discriminate H|].
    apply IH. exact H.
Qed.

Lemma utxo_remove_lookup_none :
  forall (u : UTXOSet) (op : OutPoint), utxo_lookup (utxo_remove u op) op = None.
Proof.
  intros u op. unfold utxo_remove.
  induction u as [|[op' o'] t IH].
  - reflexivity.
  - simpl. destruct (outpoint_eqb op' op) eqn:E; simpl.
    + exact IH.
    + destruct (outpoint_eqb op op') eqn:E2.
      * apply outpoint_eqb_eq in E2. subst op.
        rewrite outpoint_eqb_refl in E. discriminate E.
      * exact IH.
Qed.

Lemma utxo_insert_lookup :
  forall (u : UTXOSet) (op : OutPoint) (out : TxOut),
    utxo_lookup (utxo_insert u op out) op = Some out.
Proof.
  intros u op out. unfold utxo_insert.
  rewrite utxo_lookup_app_none by apply utxo_remove_lookup_none.
  simpl. rewrite outpoint_eqb_refl. reflexivity.
Qed.

(** Absence is preserved by further removals. *)
Lemma utxo_lookup_remove_stable :
  forall (u : UTXOSet) (op q : OutPoint),
    utxo_lookup u op = None -> utxo_lookup (utxo_remove u q) op = None.
Proof.
  intros u op q. unfold utxo_remove.
  induction u as [|[op' o'] t IH]; intros H.
  - reflexivity.
  - simpl in H. destruct (outpoint_eqb op op') eqn:E; [discriminate H|].
    simpl. destruct (outpoint_eqb op' q); simpl.
    + apply IH. exact H.
    + rewrite E. apply IH. exact H.
Qed.

(** Absence is preserved by inserts at a different outpoint. *)
Lemma utxo_lookup_insert_stable :
  forall (u : UTXOSet) (op q : OutPoint) (o : TxOut),
    utxo_lookup u op = None ->
    op <> q ->
    utxo_lookup (utxo_insert u q o) op = None.
Proof.
  intros u op q o H Hne. unfold utxo_insert.
  rewrite utxo_lookup_app_none
    by (apply utxo_lookup_remove_stable; exact H).
  simpl. rewrite (outpoint_eqb_false op q Hne). reflexivity.
Qed.

Lemma utxo_lookup_fold_remove_stable :
  forall (l : list OutPoint) (u : UTXOSet) (op : OutPoint),
    utxo_lookup u op = None ->
    utxo_lookup (fold_left (fun u' q => utxo_remove u' q) l u) op = None.
Proof.
  induction l as [|q rest IH]; intros u op H; simpl.
  - exact H.
  - apply IH. apply utxo_lookup_remove_stable. exact H.
Qed.

(** Every spent outpoint is absent after the removal phase of [apply_tx_utxo]. *)
Theorem spends_removed_by_fold :
  forall (l : list OutPoint) (u : UTXOSet) (op : OutPoint),
    In op l ->
    utxo_lookup (fold_left (fun u' q => utxo_remove u' q) l u) op = None.
Proof.
  induction l as [|q rest IH]; intros u op Hin; simpl.
  - inversion Hin.
  - destruct Hin as [Heq | Hin'].
    + subst q. apply utxo_lookup_fold_remove_stable.
      apply utxo_remove_lookup_none.
    + apply IH. exact Hin'.
Qed.

(** [apply_tx_utxo] gives created outputs these synthetic outpoints. *)
Definition synthetic_outpoint (i : nat) : OutPoint :=
  MkOutPoint hash32_placeholder i.

(** [op] is not re-created by [tx] when applied at index [tx_index]. *)
Definition not_recreated (tx : TxAbs) (tx_index : nat) (op : OutPoint) : Prop :=
  forall i, (i < length (tx_creates tx))%nat ->
            op <> synthetic_outpoint (tx_index + i).

Lemma utxo_lookup_fold_create_stable :
  forall (creates : list TxOut) (u : UTXOSet) (base : nat) (op : OutPoint),
    utxo_lookup u op = None ->
    (forall i, (i < length creates)%nat -> op <> synthetic_outpoint (base + i)) ->
    utxo_lookup
      (fst (fold_left
              (fun (acc : UTXOSet * nat) (o : TxOut) =>
                 let (u', idx) := acc in
                 (utxo_insert u' (MkOutPoint hash32_placeholder idx) o, S idx))
              creates (u, base))) op = None.
Proof.
  induction creates as [|o rest IH]; intros u base op H Hne; simpl.
  - exact H.
  - apply IH.
    + apply utxo_lookup_insert_stable; [exact H|].
      assert (Hb : op <> synthetic_outpoint (base + 0)) by (apply Hne; simpl; lia).
      rewrite Nat.add_0_r in Hb. unfold synthetic_outpoint in Hb. exact Hb.
    + intros i Hi.
      replace (S base + i)%nat with (base + S i)%nat by lia.
      apply Hne. simpl. lia.
Qed.

(** ----------------------------------------------------------------------
    SECURITY-001-F / F-3, F-4.

    The historical unconditional statement of [utxo_spend_removes_output]
    was [Admitted]. It is not merely unproven — it is REFUTABLE in this model:
    [apply_tx_utxo] assigns each created output the synthetic outpoint
    [MkOutPoint hash32_placeholder (tx_index + i)], which can collide with an
    outpoint the very same transaction spends. The refutation is below, and the
    theorem is restated with the [not_recreated] side condition that makes it
    true. Both are now closed with [Qed].
    ---------------------------------------------------------------------- *)

Definition ce_out : TxOut := {| out_value := 5%Z; out_script := [] |}.
Definition ce_op : OutPoint := MkOutPoint hash32_placeholder 0.
Definition ce_u : UTXOSet := [(ce_op, ce_out)].
Definition ce_tx : TxAbs := {| tx_spends := [ce_op]; tx_creates := [ce_out] |}.

Lemma ce_apply :
  apply_tx_utxo ce_u ce_tx 0 = Some (utxo_insert (utxo_remove ce_u ce_op) ce_op ce_out).
Proof.
  unfold apply_tx_utxo, ce_tx. cbv zeta.
  cbn [tx_spends tx_creates forallb fold_left fst].
  unfold ce_u at 1. cbn [utxo_lookup].
  rewrite outpoint_eqb_refl. cbn [andb].
  reflexivity.
Qed.

Theorem utxo_spend_removes_output_unconditional_is_false :
  ~ (forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (op : OutPoint) (u' : UTXOSet),
        In op (tx_spends tx) ->
        apply_tx_utxo u tx idx = Some u' ->
        utxo_lookup u' op = None).
Proof.
  intros Hbad.
  assert (Hin : In ce_op (tx_spends ce_tx))
    by (unfold ce_tx; cbn [tx_spends]; left; reflexivity).
  specialize (Hbad ce_u ce_tx 0 ce_op _ Hin ce_apply).
  rewrite utxo_insert_lookup in Hbad. discriminate Hbad.
Qed.

(** Corrected form: a spent outpoint is absent afterwards unless the same
    transaction re-creates it at a colliding synthetic outpoint. *)
Theorem utxo_spend_removes_output :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (op : OutPoint) (u' : UTXOSet),
    In op (tx_spends tx) ->
    not_recreated tx idx op ->
    apply_tx_utxo u tx idx = Some u' ->
    utxo_lookup u' op = None.
Proof.
  intros u tx idx op u' Hin Hnr Happ.
  unfold apply_tx_utxo in Happ. cbv zeta in Happ.
  match type of Happ with
  | (if ?b then _ else _) = _ => destruct b eqn:Hok
  end; [|discriminate Happ].
  inversion Happ as [Hu']; clear Happ. subst u'.
  apply utxo_lookup_fold_create_stable.
  - apply spends_removed_by_fold. exact Hin.
  - exact Hnr.
Qed.

Theorem utxo_create_adds_output :
  forall (u : UTXOSet) (op : OutPoint) (out : TxOut),
    utxo_lookup (utxo_insert u op out) op = Some out.
Proof.
  intros. exact (utxo_insert_lookup u op out).
Qed.

Theorem rollback_restores_previous_state :
  forall (s : ReplayState) (k : nat) (snap : ReplaySnapshot) (s' : ReplayState),
    k > 0 ->
    rollback_from_history (rs_history s) k = Some snap ->
    rollback s k = Some s' ->
    rs_utxo s' = snap_utxo snap /\
    rs_chain s' = snap_chain snap /\
    rs_oru_supply s' = snap_oru_supply snap /\
    rs_res_supply s' = snap_res_supply snap.
Proof.
  intros s k snap s' Hk Hhist Hrb.
  destruct k as [|k']; [inversion Hk|].
  unfold rollback in Hrb. rewrite Hhist in Hrb.
  inversion Hrb as [Hs']; clear Hrb. subst s'.
  cbn [rs_utxo rs_chain rs_oru_supply rs_res_supply state_of_snapshot].
  repeat split.
Qed.

Theorem utxo_non_negative_init : utxo_non_negative (rs_utxo init_replay_state).
Proof.
  unfold init_replay_state. exact utxo_non_negative_empty.
Qed.

(** Block replay preserves spend/create shape (extends single-tx lemmas).
    Carries the same [not_recreated] side condition as
    [utxo_spend_removes_output]; the unconditional form is refuted above. *)
Theorem replay_preserves_utxo_consistency :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState) (op : OutPoint),
    In op (tx_spends tx) ->
    not_recreated tx idx op ->
    apply_tx s tx idx = Some s' ->
    utxo_lookup (rs_utxo s') op = None.
Proof.
  intros s tx idx s' op Hin Hnr Happ.
  unfold apply_tx in Happ.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|] eqn:E; [|discriminate Happ].
  inversion Happ as [Hs']; clear Happ. subst s'.
  cbn [rs_utxo replay_set_utxo].
  exact (utxo_spend_removes_output (rs_utxo s) tx idx op u' Hin Hnr E).
Qed.
