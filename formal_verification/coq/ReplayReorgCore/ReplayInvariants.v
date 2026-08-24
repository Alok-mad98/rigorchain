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

(** Created outputs of [tx] are [(tx_id tx, 0) .. (tx_id tx, n-1)]. *)
Definition created_outpoint (tx : TxAbs) (i : nat) : OutPoint :=
  MkOutPoint (tx_id tx) i.

(** [op] is not among the outputs [tx] creates. Independent of in-block index. *)
Definition not_recreated (tx : TxAbs) (op : OutPoint) : Prop :=
  forall i, (i < length (tx_creates tx))%nat ->
            op <> created_outpoint tx i.

(** Distinct transaction identity implies [not_recreated]: a spend of a
    previous tx's outpoint cannot collide with this tx's creates. *)
Lemma distinct_txid_not_recreated :
  forall (tx : TxAbs) (op : OutPoint),
    outpoint_txid op <> tx_id tx ->
    not_recreated tx op.
Proof.
  intros tx op Hne i Hi Heq.
  unfold created_outpoint in Heq.
  subst op.
  unfold outpoint_txid in Hne.
  apply Hne. reflexivity.
Qed.

Lemma utxo_lookup_fold_create_stable :
  forall (creates : list TxOut) (u : UTXOSet) (base : nat) (hid : Hash32)
         (op : OutPoint),
    utxo_lookup u op = None ->
    (forall i, (i < length creates)%nat -> op <> MkOutPoint hid (base + i)) ->
    utxo_lookup (fold_create_outputs creates u hid base) op = None.
Proof.
  unfold fold_create_outputs.
  induction creates as [|o rest IH]; intros u base hid op H Hne; simpl.
  - exact H.
  - apply IH.
    + apply utxo_lookup_insert_stable; [exact H|].
      assert (Hb : op <> MkOutPoint hid (base + 0)) by (apply Hne; simpl; lia).
      rewrite Nat.add_0_r in Hb. exact Hb.
    + intros i Hi.
      replace (S base + i)%nat with (base + S i)%nat by lia.
      apply Hne. simpl. lia.
Qed.

(** ----------------------------------------------------------------------
    Tightening 2026-08-18 (G-5).

    Created outputs now use [tx_id tx] with per-tx vout 0..n-1, not a
    shared [hash32_placeholder] plus a running index. The natural
    production case — spending a previous transaction's outpoint — is
    therefore unconditional: [utxo_spend_removes_output_distinct_txid].

    The fully universal statement (no side condition) remains false, but
    only under *txid reuse*: a transaction whose [tx_id] equals the txid
    of an outpoint it spends can re-insert that outpoint. That is the
    BIP30-class duplicate-txid edge, not a modelling shortcut. The
    refutation below uses that edge, not the old placeholder collision.
    ---------------------------------------------------------------------- *)

Definition ce_out : TxOut :=
  {| out_value := 5%Z; out_script := spk_oru |}.
Definition ce_op : OutPoint := MkOutPoint hash32_placeholder 0.
Definition ce_u : UTXOSet := [(ce_op, ce_out)].
(** Duplicate-txid transaction: spends and re-creates [(tx_id, 0)]. *)
Definition ce_tx : TxAbs :=
  {| tx_id := hash32_placeholder; tx_spends := [ce_op]; tx_creates := [ce_out];
     tx_pcorb := None |}.

Lemma ce_apply :
  apply_tx_utxo ce_u ce_tx 0 = Some (utxo_insert (utxo_remove ce_u ce_op) ce_op ce_out).
Proof.
  unfold apply_tx_utxo, fold_create_outputs, ce_tx. cbv zeta.
  cbn [tx_id tx_spends tx_creates forallb fold_left fst].
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

(** Spent [op] is absent afterwards unless [tx] re-creates the same outpoint
    (duplicate [tx_id] + overlapping vout). *)
Theorem utxo_spend_removes_output :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (op : OutPoint) (u' : UTXOSet),
    In op (tx_spends tx) ->
    not_recreated tx op ->
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
  - intros i Hi. unfold not_recreated, created_outpoint in Hnr.
    apply Hnr. exact Hi.
Qed.

(** Production case: spending a *different* transaction's outpoint always
    deletes it. [not_recreated] is derived, not assumed. *)
Theorem utxo_spend_removes_output_distinct_txid :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (op : OutPoint) (u' : UTXOSet),
    In op (tx_spends tx) ->
    outpoint_txid op <> tx_id tx ->
    apply_tx_utxo u tx idx = Some u' ->
    utxo_lookup u' op = None.
Proof.
  intros u tx idx op u' Hin Hne Happ.
  eapply utxo_spend_removes_output; [exact Hin | | exact Happ].
  apply distinct_txid_not_recreated. exact Hne.
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
    [not_recreated] is now independent of in-block index. The production
    case [outpoint_txid op <> tx_id tx] discharges it via
    [distinct_txid_not_recreated]. *)
Theorem replay_preserves_utxo_consistency :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState) (op : OutPoint),
    In op (tx_spends tx) ->
    not_recreated tx op ->
    apply_tx s tx idx = Some s' ->
    utxo_lookup (rs_utxo s') op = None.
Proof.
  intros s tx idx s' op Hin Hnr Happ.
  apply apply_tx_some_inv in Happ.
  destruct Happ as [u' [E [_ [_ Hs]]]]. subst s'.
  cbn [rs_utxo replay_set_utxo].
  exact (utxo_spend_removes_output (rs_utxo s) tx idx op u' Hin Hnr E).
Qed.

Theorem replay_preserves_utxo_consistency_distinct_txid :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState) (op : OutPoint),
    In op (tx_spends tx) ->
    outpoint_txid op <> tx_id tx ->
    apply_tx s tx idx = Some s' ->
    utxo_lookup (rs_utxo s') op = None.
Proof.
  intros s tx idx s' op Hin Hne Happ.
  eapply replay_preserves_utxo_consistency; [exact Hin | | exact Happ].
  apply distinct_txid_not_recreated. exact Hne.
Qed.
