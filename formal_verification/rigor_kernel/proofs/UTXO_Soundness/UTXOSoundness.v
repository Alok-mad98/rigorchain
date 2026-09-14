(** RigorKernel.proofs.UTXO_Soundness.UTXOSoundness — P0-108 UTXO kernel.

    Scope: consensus kernel only.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Core.Amount.

Local Open Scope Z_scope.

Definition hash32 := list byte.

Record outpoint := { op_txid : hash32; op_index : nat }.

Record txout := { txo_value : Z; txo_spk : list byte }.

Record tx := {
  tx_id : hash32;
  tx_in : list outpoint;
  tx_out : list txout;
  tx_coinbase : bool
}.

Definition dummy_txout : txout := {| txo_value := 0; txo_spk := nil |}.

Fixpoint bytes_eqb (a b : list byte) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (Nat.eqb x y) (bytes_eqb xs ys)
  | _, _ => false
  end.

Definition outpoint_eqb (o1 o2 : outpoint) : bool :=
  andb (bytes_eqb (op_txid o1) (op_txid o2))
       (Nat.eqb (op_index o1) (op_index o2)).

Lemma bytes_eqb_refl : forall a, bytes_eqb a a = true.
Proof.
  induction a as [|x xs IH]; simpl; [reflexivity|].
  rewrite Nat.eqb_refl, IH. reflexivity.
Qed.

Lemma bytes_eqb_ok :
  forall a b, bytes_eqb a b = true <-> a = b.
Proof.
  induction a as [|x xs IH]; intros b; destruct b as [|y ys]; simpl.
  - split; reflexivity.
  - split; discriminate.
  - split; discriminate.
  - rewrite andb_true_iff, Nat.eqb_eq, IH.
    split.
    + intros [Hxy Hrest]. subst. reflexivity.
    + intros H. inversion H. subst. split; reflexivity.
Qed.

Lemma outpoint_eqb_refl : forall o, outpoint_eqb o o = true.
Proof.
  intros [t i]. unfold outpoint_eqb. simpl.
  rewrite bytes_eqb_refl, Nat.eqb_refl. reflexivity.
Qed.

Lemma outpoint_eqb_ok :
  forall o1 o2, outpoint_eqb o1 o2 = true <-> o1 = o2.
Proof.
  intros [t1 i1] [t2 i2]. unfold outpoint_eqb. simpl.
  rewrite andb_true_iff, bytes_eqb_ok, Nat.eqb_eq.
  split.
  - intros [Ht Hi]. subst. reflexivity.
  - intros H. inversion H. subst. split; reflexivity.
Qed.

Lemma outpoint_eqb_false :
  forall o1 o2, outpoint_eqb o1 o2 = false <-> o1 <> o2.
Proof.
  intros o1 o2.
  destruct (outpoint_eqb o1 o2) eqn:Heq.
  - apply outpoint_eqb_ok in Heq. subst.
    split; [discriminate| intros Hc; congruence].
  - split; [intros _ H; subst; rewrite outpoint_eqb_refl in Heq; discriminate|].
    intros. reflexivity.
Qed.

Definition utxo_set := list (outpoint * txout).
Definition utxo_wf (u : utxo_set) : Prop := NoDup (map fst u).

Fixpoint utxo_lookup (u : utxo_set) (o : outpoint) : option txout :=
  match u with
  | nil => None
  | (o', c) :: rest =>
      if outpoint_eqb o o' then Some c else utxo_lookup rest o
  end.

Fixpoint utxo_remove (u : utxo_set) (o : outpoint) : utxo_set :=
  match u with
  | nil => nil
  | (o', c) :: rest =>
      if outpoint_eqb o o' then rest
      else (o', c) :: utxo_remove rest o
  end.

Fixpoint utxo_remove_all (u : utxo_set) (os : list outpoint) : utxo_set :=
  match os with
  | nil => u
  | o :: rest => utxo_remove_all (utxo_remove u o) rest
  end.

Fixpoint utxo_insert_outputs (u : utxo_set) (tid : hash32)
         (outs : list txout) (i : nat) : utxo_set :=
  match outs with
  | nil => u
  | c :: rest =>
      ({| op_txid := tid; op_index := i |}, c)
        :: utxo_insert_outputs u tid rest (S i)
  end.

Fixpoint output_outpoints (tid : hash32) (n : nat) : list outpoint :=
  match n with
  | O => nil
  | S n' => output_outpoints tid n' ++ [{| op_txid := tid; op_index := n' |}]
  end.

Definition is_output_op (t : tx) (o : outpoint) : Prop :=
  op_txid o = tx_id t /\ (op_index o < length (tx_out t))%nat.

Definition inputs_fresh (t : tx) : Prop :=
  forall o, In o (tx_in t) -> ~ is_output_op t o.

Definition is_output_op_b (t : tx) (o : outpoint) : bool :=
  andb (bytes_eqb (op_txid o) (tx_id t))
       (Nat.ltb (op_index o) (length (tx_out t))).

Definition inputs_fresh_b (t : tx) : bool :=
  forallb (fun o => negb (is_output_op_b t o)) (tx_in t).

Fixpoint sum_values (outs : list txout) : Z :=
  match outs with
  | nil => 0
  | o :: rest => txo_value o + sum_values rest
  end.

Fixpoint nodupb (l : list outpoint) : bool :=
  match l with
  | nil => true
  | x :: xs => andb (negb (existsb (outpoint_eqb x) xs)) (nodupb xs)
  end.

Definition nonemptyb {A} (l : list A) : bool :=
  match l with nil => false | _ => true end.

Definition valid_tx_b (t : tx) : bool :=
  andb (negb (tx_coinbase t))
  (andb (nonemptyb (tx_in t))
  (andb (nonemptyb (tx_out t))
  (andb (nodupb (tx_in t))
  (andb (forallb (fun o => money_range_b (txo_value o)) (tx_out t))
  (andb (money_range_b (sum_values (tx_out t)))
        (inputs_fresh_b t)))))).

Definition valid_tx (t : tx) : Prop :=
  tx_coinbase t = false /\
  tx_in t <> nil /\
  tx_out t <> nil /\
  NoDup (tx_in t) /\
  Forall (fun o => money_range (txo_value o)) (tx_out t) /\
  money_range (sum_values (tx_out t)) /\
  inputs_fresh t.

Definition apply_body (u : utxo_set) (t : tx) : utxo_set :=
  let u1 := utxo_remove_all u (tx_in t) in
  let u2 := utxo_remove_all u1 (output_outpoints (tx_id t) (length (tx_out t))) in
  utxo_insert_outputs u2 (tx_id t) (tx_out t) 0%nat.

Definition apply_tx (u : utxo_set) (t : tx) : option utxo_set :=
  if valid_tx_b t then Some (apply_body u t) else None.

Definition inputs_consumed (t : tx) (u u' : utxo_set) : Prop :=
  forall o, In o (tx_in t) -> utxo_lookup u' o = None.

Definition outputs_created (t : tx) (u u' : utxo_set) : Prop :=
  forall i,
    (i < length (tx_out t))%nat ->
    utxo_lookup u' {| op_txid := tx_id t; op_index := i |} =
    Some (nth i (tx_out t) dummy_txout).

Definition apply_tx_opt (acc : option utxo_set) (t : tx) : option utxo_set :=
  match acc with
  | None => None
  | Some u => apply_tx u t
  end.

Fixpoint collect_inputs (b : list tx) : list outpoint :=
  match b with
  | nil => nil
  | t :: rest => tx_in t ++ collect_inputs rest
  end.

Definition apply_block (u : utxo_set) (b : list tx) : option utxo_set :=
  if nodupb (collect_inputs b)
  then fold_left apply_tx_opt b (Some u)
  else None.

Lemma existsb_eqb_in :
  forall o l, existsb (outpoint_eqb o) l = true <-> In o l.
Proof.
  intros o l. induction l as [|x xs IH]; simpl.
  - split; [discriminate| contradiction].
  - rewrite orb_true_iff, IH.
    split.
    + intros [Heq | Hin].
      * left. apply outpoint_eqb_ok in Heq. symmetry. exact Heq.
      * right. exact Hin.
    + intros [Heq | Hin].
      * left. subst x. apply outpoint_eqb_refl.
      * right. exact Hin.
Qed.

Lemma existsb_eqb_nIn :
  forall o l, existsb (outpoint_eqb o) l = false <-> ~ In o l.
Proof.
  intros o l.
  destruct (existsb (outpoint_eqb o) l) eqn:He.
  - apply existsb_eqb_in in He.
    split; [discriminate| intros Hn; contradiction].
  - split; [|intros; reflexivity].
    intros _ Hin. apply existsb_eqb_in in Hin. congruence.
Qed.

Lemma nodupb_ok :
  forall l, nodupb l = true <-> NoDup l.
Proof.
  induction l as [|x xs IH]; simpl.
  - split; intros; [constructor| reflexivity].
  - rewrite andb_true_iff, negb_true_iff, IH.
    split.
    + intros [Hnin Hnd]. apply existsb_eqb_nIn in Hnin. constructor; assumption.
    + intros Hnd. inversion Hnd. subst.
      split; [apply existsb_eqb_nIn; exact H1 | exact H2].
Qed.

Lemma money_range_b_ok :
  forall v, money_range_b v = true <-> money_range v.
Proof.
  intros v. unfold money_range_b, money_range.
  rewrite andb_true_iff, Z.leb_le, Z.leb_le. tauto.
Qed.

Lemma is_output_op_b_ok :
  forall t o, is_output_op_b t o = true <-> is_output_op t o.
Proof.
  intros t o. unfold is_output_op_b, is_output_op.
  rewrite andb_true_iff, bytes_eqb_ok, Nat.ltb_lt. tauto.
Qed.

Lemma inputs_fresh_b_ok :
  forall t, inputs_fresh_b t = true <-> inputs_fresh t.
Proof.
  intros t. unfold inputs_fresh_b, inputs_fresh.
  rewrite forallb_forall.
  split.
  - intros H o Hin Hop.
    specialize (H o Hin). apply negb_true_iff in H.
    apply is_output_op_b_ok in Hop. congruence.
  - intros H o Hin. apply negb_true_iff.
    destruct (is_output_op_b t o) eqn:Hb; [|reflexivity].
    apply is_output_op_b_ok in Hb. exfalso. apply (H o Hin Hb).
Qed.

Lemma nonemptyb_ok :
  forall {A} (l : list A), nonemptyb l = true <-> l <> nil.
Proof.
  intros A l. destruct l; simpl; split; intros; congruence || discriminate.
Qed.

Lemma forallb_money_ok :
  forall outs,
    forallb (fun o => money_range_b (txo_value o)) outs = true <->
    Forall (fun o => money_range (txo_value o)) outs.
Proof.
  intros outs. rewrite forallb_forall, Forall_forall.
  split; intros H o Hin; apply money_range_b_ok; apply H; exact Hin.
Qed.

Lemma valid_tx_b_reflect :
  forall t, valid_tx_b t = true <-> valid_tx t.
Proof.
  intros t. unfold valid_tx_b, valid_tx.
  destruct (tx_coinbase t) eqn:Hc; simpl.
  - split; [intros H; discriminate | intros [H _]; discriminate].
  - rewrite !andb_true_iff.
    rewrite nonemptyb_ok, nonemptyb_ok, nodupb_ok, forallb_money_ok,
            money_range_b_ok, inputs_fresh_b_ok.
    tauto.
Qed.

Lemma lookup_none_not_in :
  forall u o, utxo_lookup u o = None -> ~ In o (map fst u).
Proof.
  intros u o. induction u as [|[p c] rest IH]; simpl; [tauto|].
  destruct (outpoint_eqb o p) eqn:Heq.
  - intros H. discriminate.
  - intros H [He | Hin].
    + subst p. rewrite outpoint_eqb_refl in Heq. discriminate.
    + apply IH; assumption.
Qed.

Lemma not_in_lookup_none :
  forall u o, ~ In o (map fst u) -> utxo_lookup u o = None.
Proof.
  intros u o. induction u as [|[p c] rest IH]; simpl; [reflexivity|].
  intros Hnin.
  destruct (outpoint_eqb o p) eqn:Heq.
  - apply outpoint_eqb_ok in Heq. subst p. simpl in Hnin. tauto.
  - apply IH. intros Hin. apply Hnin. right. exact Hin.
Qed.

Lemma remove_fst_subset :
  forall u x o,
    In o (map fst (utxo_remove u x)) ->
    In o (map fst u).
Proof.
  intros u x o. induction u as [|[p c] rest IH]; simpl; [tauto|].
  destruct (outpoint_eqb x p) eqn:Heq.
  - intros Hin. right. exact Hin.
  - simpl. intros [He | Hin].
    + left. exact He.
    + right. apply IH. exact Hin.
Qed.

Lemma remove_drops_key :
  forall u x, utxo_wf u -> ~ In x (map fst (utxo_remove u x)).
Proof.
  intros u x Hwf. unfold utxo_wf in Hwf.
  induction u as [|[p c] rest IH]; simpl; [tauto|].
  simpl in Hwf. inversion Hwf; subst.
  destruct (outpoint_eqb x p) eqn:Heq.
  - apply outpoint_eqb_ok in Heq. subst p. exact H1.
  - simpl. intros [He | Hin].
    + subst p. rewrite outpoint_eqb_refl in Heq. discriminate.
    + apply IH; [exact H2 | exact Hin].
Qed.

Lemma remove_preserves_wf :
  forall u o, utxo_wf u -> utxo_wf (utxo_remove u o).
Proof.
  intros u o Hwf. unfold utxo_wf in *.
  induction u as [|[p c] rest IH]; simpl; [constructor|].
  simpl in Hwf. inversion Hwf; subst.
  destruct (outpoint_eqb o p) eqn:Heq; [exact H2|].
  simpl. constructor.
  - intros Hin. apply H1. eapply remove_fst_subset. exact Hin.
  - apply IH. exact H2.
Qed.

Lemma remove_all_preserves_wf :
  forall os u, utxo_wf u -> utxo_wf (utxo_remove_all u os).
Proof.
  intros os. induction os as [|o os IH]; intros u Hwf; simpl; [exact Hwf|].
  apply IH. apply remove_preserves_wf. exact Hwf.
Qed.

(** Statement: removing [o] does not change lookup of a different key.
    Assumptions: [o <> o'].
    Consensus kernel only: yes. *)
Theorem utxo_lookup_remove_neq :
  forall u o o',
    o <> o' ->
    utxo_lookup (utxo_remove u o) o' = utxo_lookup u o'.
Proof.
  intros u o o' Hne.
  induction u as [|[p c] rest IH]; simpl; [reflexivity|].
  destruct (outpoint_eqb o p) eqn:Hop.
  - apply outpoint_eqb_ok in Hop. subst p.
    destruct (outpoint_eqb o' o) eqn:Ho'.
    + apply outpoint_eqb_ok in Ho'. subst. contradiction.
    + reflexivity.
  - simpl. destruct (outpoint_eqb o' p); [reflexivity| exact IH].
Qed.

Lemma lookup_remove_same :
  forall u o, utxo_wf u -> utxo_lookup (utxo_remove u o) o = None.
Proof.
  intros u o Hwf. apply not_in_lookup_none. apply remove_drops_key. exact Hwf.
Qed.

Lemma lookup_none_remove :
  forall u o x,
    utxo_lookup u o = None ->
    utxo_lookup (utxo_remove u x) o = None.
Proof.
  intros u o x Hnone.
  apply not_in_lookup_none.
  intros Hin. apply lookup_none_not_in in Hnone.
  apply Hnone. eapply remove_fst_subset. exact Hin.
Qed.

Lemma lookup_none_remove_all :
  forall os u o,
    utxo_lookup u o = None ->
    utxo_lookup (utxo_remove_all u os) o = None.
Proof.
  intros os. induction os as [|x xs IH]; intros u o Hnone; simpl; [exact Hnone|].
  apply IH. apply lookup_none_remove. exact Hnone.
Qed.

(** Statement: every outpoint in [os] is absent after [utxo_remove_all].
    Assumptions: [utxo_wf u] and [In o os].
    Consensus kernel only: yes. *)
Theorem utxo_remove_all_spent :
  forall os u o,
    utxo_wf u ->
    In o os ->
    utxo_lookup (utxo_remove_all u os) o = None.
Proof.
  intros os. induction os as [|x xs IH]; intros u o Hwf Hin; simpl in *.
  - contradiction.
  - destruct Hin as [He | Hin].
    + subst x. apply lookup_none_remove_all. apply lookup_remove_same. exact Hwf.
    + apply IH; [apply remove_preserves_wf; exact Hwf | exact Hin].
Qed.

Lemma lookup_remove_all_other :
  forall os u o,
    ~ In o os ->
    utxo_lookup (utxo_remove_all u os) o = utxo_lookup u o.
Proof.
  intros os. induction os as [|x xs IH]; intros u o Hnin; simpl; [reflexivity|].
  rewrite IH.
  - destruct (outpoint_eqb x o) eqn:Heq.
    + apply outpoint_eqb_ok in Heq. subst x. simpl in Hnin. tauto.
    + apply utxo_lookup_remove_neq. apply outpoint_eqb_false. exact Heq.
  - intros Hin. apply Hnin. right. exact Hin.
Qed.

Lemma in_output_outpoints :
  forall tid n o,
    In o (output_outpoints tid n) <->
    op_txid o = tid /\ (op_index o < n)%nat.
Proof.
  intros tid n o. induction n as [|n IH]; simpl.
  - split; [contradiction| intros [_ H]; lia].
  - rewrite in_app_iff. simpl.
    split.
    + intros [Hin | [He | []]].
      * apply IH in Hin. destruct Hin as [Ht Hi]. split; [exact Ht| lia].
      * subst o. simpl. split; [reflexivity| lia].
    + intros [Ht Hlt].
      destruct (Nat.eq_dec (op_index o) n) as [Heq|Hne].
      * right. left. destruct o. simpl in *. subst. reflexivity.
      * left. apply IH. split; [exact Ht| lia].
Qed.

Lemma is_output_op_in :
  forall t o,
    is_output_op t o <->
    In o (output_outpoints (tx_id t) (length (tx_out t))).
Proof.
  intros t o. unfold is_output_op. rewrite in_output_outpoints. tauto.
Qed.

Lemma insert_hit :
  forall u tid outs i k d,
    (k < length outs)%nat ->
    utxo_lookup (utxo_insert_outputs u tid outs i)
                {| op_txid := tid; op_index := (i + k)%nat |} =
    Some (nth k outs d).
Proof.
  intros u tid outs i k d.
  revert i k. induction outs as [|c rest IH]; intros i k Hlt; simpl in Hlt; [lia|].
  simpl.
  destruct k as [|k'].
  - rewrite Nat.add_0_r. rewrite outpoint_eqb_refl. reflexivity.
  - destruct (outpoint_eqb {| op_txid := tid; op_index := (i + S k')%nat |}
                           {| op_txid := tid; op_index := i |}) eqn:Heq.
    + apply outpoint_eqb_ok in Heq. inversion Heq. lia.
    + replace (i + S k')%nat with (S i + k')%nat by lia.
      apply IH. lia.
Qed.

Lemma insert_miss :
  forall u tid outs i o,
    op_txid o <> tid \/
    (op_index o < i)%nat \/
    (i + length outs <= op_index o)%nat ->
    utxo_lookup (utxo_insert_outputs u tid outs i) o = utxo_lookup u o.
Proof.
  intros u tid outs i o Hmiss.
  revert i Hmiss. induction outs as [|c rest IH]; intros i Hmiss; simpl; [reflexivity|].
  destruct (outpoint_eqb o {| op_txid := tid; op_index := i |}) eqn:Heq.
  - apply outpoint_eqb_ok in Heq. subst o. simpl in Hmiss.
    destruct Hmiss as [Htid | [Hlt | Hge]]; lia || congruence.
  - apply IH.
    destruct Hmiss as [Htid | [Hlt | Hge]].
    + left. exact Htid.
    + right. left. lia.
    + right. right. simpl in Hge. lia.
Qed.

Lemma in_insert_fst :
  forall u tid outs i o,
    In o (map fst (utxo_insert_outputs u tid outs i)) ->
    In o (map fst u) \/
    (op_txid o = tid /\ (i <= op_index o < i + length outs)%nat).
Proof.
  intros u tid outs i o. revert i.
  induction outs as [|c rest IH]; intros i; simpl.
  - intros Hin. left. exact Hin.
  - intros [He | Hin].
    + subst o. simpl. right. split; [reflexivity| lia].
    + apply IH in Hin. destruct Hin as [Hin | [Ht [Hlo Hhi]]].
      * left. exact Hin.
      * right. split; [exact Ht| lia].
Qed.

Lemma insert_preserves_wf' :
  forall u tid outs i,
    utxo_wf u ->
    (forall k, (k < length outs)%nat ->
       ~ In {| op_txid := tid; op_index := (i + k)%nat |} (map fst u)) ->
    utxo_wf (utxo_insert_outputs u tid outs i).
Proof.
  intros u tid outs i Hwf Hfresh. unfold utxo_wf in *.
  revert i Hfresh.
  induction outs as [|c rest IH]; intros i Hfresh; simpl; [exact Hwf|].
  constructor.
  - intros Hin.
    apply in_insert_fst in Hin.
    destruct Hin as [Hin | [Ht Hrng]].
    + apply (Hfresh 0%nat); [simpl; lia| rewrite Nat.add_0_r; exact Hin].
    + simpl in Ht, Hrng. lia.
  - apply IH. intros k Hk Hin.
    apply (Hfresh (S k)); [simpl; lia|].
    replace (i + S k)%nat with (S i + k)%nat by lia.
    exact Hin.
Qed.

Lemma apply_body_wf :
  forall u t, utxo_wf u -> utxo_wf (apply_body u t).
Proof.
  intros u t Hwf. unfold apply_body.
  set (u1 := utxo_remove_all u (tx_in t)).
  set (ops := output_outpoints (tx_id t) (length (tx_out t))).
  set (u2 := utxo_remove_all u1 ops).
  assert (Hwf1 : utxo_wf u1) by (apply remove_all_preserves_wf; exact Hwf).
  assert (Hwf2 : utxo_wf u2) by (apply remove_all_preserves_wf; exact Hwf1).
  apply insert_preserves_wf'.
  - exact Hwf2.
  - intros k Hk Hin.
    apply lookup_none_not_in with
      (u := u2) (o := {| op_txid := tx_id t; op_index := (0 + k)%nat |}).
    + apply utxo_remove_all_spent; [exact Hwf1|].
      apply in_output_outpoints. split; [reflexivity| exact Hk].
    + exact Hin.
Qed.

(** Statement: a successful [apply_tx] preserves [utxo_wf].
    Assumptions: [utxo_wf u] and [apply_tx u t = Some u'].
    Consensus kernel only: yes. *)
Theorem apply_tx_preserves_wf :
  forall u t u',
    utxo_wf u ->
    apply_tx u t = Some u' ->
    utxo_wf u'.
Proof.
  intros u t u' Hwf Happ.
  unfold apply_tx in Happ.
  destruct (valid_tx_b t); [|discriminate].
  inversion Happ. subst u'. apply apply_body_wf. exact Hwf.
Qed.

Lemma apply_tx_some_valid :
  forall u t u', apply_tx u t = Some u' -> valid_tx t.
Proof.
  intros u t u' Happ. unfold apply_tx in Happ.
  destruct (valid_tx_b t) eqn:Hb; [|discriminate].
  apply valid_tx_b_reflect. exact Hb.
Qed.

Lemma apply_tx_body :
  forall u t u', apply_tx u t = Some u' -> u' = apply_body u t.
Proof.
  intros u t u' Happ. unfold apply_tx in Happ.
  destruct (valid_tx_b t); [|discriminate]. inversion Happ. reflexivity.
Qed.

(** Statement: applied inputs are absent from the post-state.
    Assumptions: [utxo_wf u] and [apply_tx u t = Some u'].
    Consensus kernel only: yes. *)
Theorem apply_tx_inputs_consumed :
  forall u t u',
    utxo_wf u ->
    apply_tx u t = Some u' ->
    inputs_consumed t u u'.
Proof.
  intros u t u' Hwf Happ o Hin.
  pose proof (apply_tx_some_valid u t u' Happ) as Hv.
  apply apply_tx_body in Happ. subst u'.
  unfold apply_body, inputs_consumed.
  set (u1 := utxo_remove_all u (tx_in t)).
  set (ops := output_outpoints (tx_id t) (length (tx_out t))).
  set (u2 := utxo_remove_all u1 ops).
  assert (Hfresh : ~ is_output_op t o).
  { unfold valid_tx in Hv. destruct Hv as [_ [_ [_ [_ [_ [_ Hf]]]]]]. apply Hf. exact Hin. }
  rewrite insert_miss.
  - apply lookup_none_remove_all.
    apply utxo_remove_all_spent; [exact Hwf | exact Hin].
  - unfold is_output_op in Hfresh.
    destruct (bytes_eqb (op_txid o) (tx_id t)) eqn:Ht.
    + apply bytes_eqb_ok in Ht.
      right. right. simpl.
      destruct (le_lt_dec (length (tx_out t)) (op_index o)) as [Hge|Hlt]; [lia|].
      exfalso. apply Hfresh. split; [exact Ht | exact Hlt].
    + left. intros Heq. apply bytes_eqb_ok in Heq. rewrite Heq in Ht. discriminate.
Qed.

(** Statement: applied outputs are present in the post-state.
    Assumptions: [apply_tx u t = Some u'].
    Consensus kernel only: yes. *)
Theorem apply_tx_outputs_created :
  forall u t u',
    apply_tx u t = Some u' ->
    outputs_created t u u'.
Proof.
  intros u t u' Happ i Hlt.
  apply apply_tx_body in Happ. subst u'.
  unfold apply_body, outputs_created.
  rewrite <- (Nat.add_0_l i) at 1.
  apply insert_hit. exact Hlt.
Qed.

(** Statement: outpoints outside the transaction frame keep their value.
    Assumptions: [apply_tx u t = Some u'], [~ In o (tx_in t)], not an output op.
    Consensus kernel only: yes. *)
Theorem apply_tx_frame :
  forall u t u' o,
    apply_tx u t = Some u' ->
    ~ In o (tx_in t) ->
    ~ is_output_op t o ->
    utxo_lookup u' o = utxo_lookup u o.
Proof.
  intros u t u' o Happ Hnin Hnout.
  apply apply_tx_body in Happ. subst u'.
  unfold apply_body.
  set (u1 := utxo_remove_all u (tx_in t)).
  set (ops := output_outpoints (tx_id t) (length (tx_out t))).
  set (u2 := utxo_remove_all u1 ops).
  rewrite insert_miss.
  - unfold u2, u1. rewrite lookup_remove_all_other.
    + apply lookup_remove_all_other. exact Hnin.
    + intros Hinops. apply Hnout. apply is_output_op_in. exact Hinops.
  - unfold is_output_op in Hnout.
    destruct (bytes_eqb (op_txid o) (tx_id t)) eqn:Ht.
    + apply bytes_eqb_ok in Ht.
      right. right. simpl.
      destruct (le_lt_dec (length (tx_out t)) (op_index o)) as [Hge|Hlt]; [lia|].
      exfalso. apply Hnout. split; [exact Ht | exact Hlt].
    + left. intros Heq. apply bytes_eqb_ok in Heq. rewrite Heq in Ht. discriminate.
Qed.

Lemma existsb_app :
  forall f (a b : list outpoint),
    existsb f (a ++ b) = orb (existsb f a) (existsb f b).
Proof.
  intros f a. induction a as [|x xs IH]; intros b; simpl; [reflexivity|].
  rewrite IH, orb_assoc. reflexivity.
Qed.

Lemma nodupb_both_false :
  forall l1 l2 o,
    In o l1 ->
    In o l2 ->
    nodupb (l1 ++ l2) = false.
Proof.
  intros l1. induction l1 as [|x xs IH]; intros l2 o Hin1 Hin2; simpl in *.
  - contradiction.
  - destruct Hin1 as [Heq | Hin1].
    + subst x. rewrite existsb_app.
      assert (H : existsb (outpoint_eqb o) l2 = true)
        by (apply existsb_eqb_in; exact Hin2).
      rewrite H. destruct (existsb (outpoint_eqb o) xs); reflexivity.
    + destruct (existsb (outpoint_eqb x) (xs ++ l2)); simpl; [reflexivity|].
      apply (IH l2 o Hin1 Hin2).
Qed.

(** Statement: a repeated outpoint across two block transactions is rejected.
    Assumptions: [In o (tx_in t1)] and [In o (tx_in t2)].
    Consensus kernel only: yes. *)
Theorem apply_block_no_double_spend :
  forall u t1 t2 o,
    In o (tx_in t1) ->
    In o (tx_in t2) ->
    apply_block u (t1 :: t2 :: nil) = None.
Proof.
  intros u t1 t2 o Hin1 Hin2.
  unfold apply_block. simpl.
  rewrite app_nil_r.
  rewrite (nodupb_both_false (tx_in t1) (tx_in t2) o Hin1 Hin2).
  reflexivity.
Qed.

Lemma fold_apply_wf :
  forall b acc,
    match acc with Some u => utxo_wf u | None => True end ->
    match fold_left apply_tx_opt b acc with
    | Some u' => utxo_wf u'
    | None => True
    end.
Proof.
  intros b. induction b as [|t ts IH]; intros acc Hacc; simpl; [exact Hacc|].
  apply IH. unfold apply_tx_opt.
  destruct acc as [u|]; [|exact I].
  destruct (apply_tx u t) as [u'|] eqn:Hap; [|exact I].
  eapply apply_tx_preserves_wf; [exact Hacc | exact Hap].
Qed.

Theorem apply_block_preserves_wf :
  forall u b u',
    utxo_wf u ->
    apply_block u b = Some u' ->
    utxo_wf u'.
Proof.
  intros u b u' Hwf Happ.
  unfold apply_block in Happ.
  destruct (nodupb (collect_inputs b)); [|discriminate].
  pose proof (fold_apply_wf b (Some u) Hwf) as Hfold.
  rewrite Happ in Hfold. exact Hfold.
Qed.

(** Statement: a valid transaction applied to a well-formed UTXO set
    produces a well-formed post-state that consumes inputs and creates outputs.
    Assumptions: [valid_tx tx] and [utxo_wf u].
    Consensus kernel only: yes. *)
Theorem utxo_soundness :
  forall tx u,
    valid_tx tx ->
    utxo_wf u ->
    exists u',
      apply_tx u tx = Some u'
      /\ utxo_wf u'
      /\ inputs_consumed tx u u'
      /\ outputs_created tx u u'.
Proof.
  intros t u Hv Hwf.
  assert (Hb : valid_tx_b t = true) by (apply valid_tx_b_reflect; exact Hv).
  exists (apply_body u t).
  assert (Hap : apply_tx u t = Some (apply_body u t)).
  { unfold apply_tx. rewrite Hb. reflexivity. }
  split; [exact Hap|].
  split; [eapply apply_tx_preserves_wf; [exact Hwf | exact Hap]|].
  split; [eapply apply_tx_inputs_consumed; [exact Hwf | exact Hap]|].
  eapply apply_tx_outputs_created. exact Hap.
Qed.
