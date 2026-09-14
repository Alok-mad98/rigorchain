(** Transaction value-flow validity. Kernel model only.

    Previously the three exported theorems here were projections of
    their own hypothesis ([intros t [_ Hb]. exact Hb.]): the conclusion
    was a conjunct of [ValueFlow], so the property was assumed rather
    than derived, and nothing connected it to a decision procedure.

    This module now (a) gives [ValueFlow] a computational counterpart
    and proves the two agree, and (b) derives consequences that are
    NOT conjuncts of the hypothesis — in particular that no single
    output can exceed total inputs, which needs induction over the
    output list.

    NOT CLAIMED: C++ CheckTransaction, signatures, script execution.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Bool.Bool.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Kernel.Transaction.Types.

Local Open Scope Z_scope.

Definition values_nonneg (t : Transaction) : Prop :=
  Forall (fun i => 0 <= in_value i) (tx_ins t) /\
  Forall (fun o => 0 <= out_value o) (tx_outs t) /\
  0 <= Fee t.

(** Named so M18 can drop the bound (allow outputs to exceed inputs). *)
Definition outputs_bounded_by_inputs (t : Transaction) : Prop :=
  inputs_total t >= outputs_total t + Fee t.

Definition ValueFlow (t : Transaction) : Prop :=
  values_nonneg t /\ outputs_bounded_by_inputs t.

(** * Structural facts about the sums

    These are proved by induction over the lists. They are not
    available by destructuring [ValueFlow]. *)

Lemma in_sum_nonneg :
  forall xs : list Input,
    Forall (fun i => 0 <= in_value i) xs -> 0 <= in_sum xs.
Proof.
  induction xs as [| x xs IH]; intros H; simpl; [lia |].
  inversion H as [| ? ? Hx Hrest]; subst.
  specialize (IH Hrest). lia.
Qed.

Lemma out_sum_nonneg :
  forall xs : list Output,
    Forall (fun o => 0 <= out_value o) xs -> 0 <= out_sum xs.
Proof.
  induction xs as [| x xs IH]; intros H; simpl; [lia |].
  inversion H as [| ? ? Hx Hrest]; subst.
  specialize (IH Hrest). lia.
Qed.

Lemma out_sum_member_le :
  forall (xs : list Output) (o : Output),
    Forall (fun y => 0 <= out_value y) xs ->
    In o xs ->
    out_value o <= out_sum xs.
Proof.
  induction xs as [| x xs IH]; intros o Hall Hin; simpl in *; [contradiction |].
  inversion Hall as [| ? ? Hx Hrest]; subst.
  destruct Hin as [-> | Hin].
  - pose proof (out_sum_nonneg xs Hrest). lia.
  - specialize (IH o Hrest Hin). lia.
Qed.

(** * Decision procedure *)

Fixpoint all_in_nonneg (xs : list Input) : bool :=
  match xs with
  | nil => true
  | i :: rest => andb (0 <=? in_value i) (all_in_nonneg rest)
  end.

Fixpoint all_out_nonneg (xs : list Output) : bool :=
  match xs with
  | nil => true
  | o :: rest => andb (0 <=? out_value o) (all_out_nonneg rest)
  end.

Definition check_value_flow (t : Transaction) : bool :=
  andb (all_in_nonneg (tx_ins t))
    (andb (all_out_nonneg (tx_outs t))
      (andb (0 <=? Fee t)
            (outputs_total t + Fee t <=? inputs_total t))).

Lemma all_in_nonneg_reflects :
  forall xs : list Input,
    all_in_nonneg xs = true <-> Forall (fun i => 0 <= in_value i) xs.
Proof.
  induction xs as [| x xs IH]; simpl.
  - split; [intros _; constructor | reflexivity].
  - split.
    + intros H. apply andb_true_iff in H as [Hx Hrest].
      constructor; [apply Z.leb_le, Hx | apply IH, Hrest].
    + intros H. inversion H as [| ? ? Hx Hrest]; subst.
      apply andb_true_iff. split; [apply Z.leb_le, Hx | apply IH, Hrest].
Qed.

Lemma all_out_nonneg_reflects :
  forall xs : list Output,
    all_out_nonneg xs = true <-> Forall (fun o => 0 <= out_value o) xs.
Proof.
  induction xs as [| x xs IH]; simpl.
  - split; [intros _; constructor | reflexivity].
  - split.
    + intros H. apply andb_true_iff in H as [Hx Hrest].
      constructor; [apply Z.leb_le, Hx | apply IH, Hrest].
    + intros H. inversion H as [| ? ? Hx Hrest]; subst.
      apply andb_true_iff. split; [apply Z.leb_le, Hx | apply IH, Hrest].
Qed.

(** The decision procedure and the invariant agree in both
    directions. This — not a hypothesis projection — is where the
    content of transaction validity now lives. *)
Theorem check_value_flow_reflects :
  forall t : Transaction, check_value_flow t = true <-> ValueFlow t.
Proof.
  intros t. unfold check_value_flow, ValueFlow, values_nonneg,
                   outputs_bounded_by_inputs.
  split.
  - intros H.
    apply andb_true_iff in H as [Hins H].
    apply andb_true_iff in H as [Houts H].
    apply andb_true_iff in H as [Hfee Hbound].
    apply Z.leb_le in Hfee. apply Z.leb_le in Hbound.
    repeat split.
    + apply all_in_nonneg_reflects, Hins.
    + apply all_out_nonneg_reflects, Houts.
    + exact Hfee.
    + lia.
  - intros [[Hins [Houts Hfee]] Hbound].
    apply all_in_nonneg_reflects in Hins.
    apply all_out_nonneg_reflects in Houts.
    rewrite Hins, Houts.
    apply Z.leb_le in Hfee. rewrite Hfee.
    assert (Hb : outputs_total t + Fee t <=? inputs_total t = true)
      by (apply Z.leb_le; lia).
    rewrite Hb. reflexivity.
Qed.

Corollary check_value_flow_false_iff :
  forall t : Transaction, check_value_flow t = false <-> ~ ValueFlow t.
Proof.
  intros t. split.
  - intros Hf Hv. apply check_value_flow_reflects in Hv.
    rewrite Hf in Hv. discriminate.
  - intros Hn. destruct (check_value_flow t) eqn:E; [| reflexivity].
    exfalso. apply Hn, check_value_flow_reflects, E.
Qed.

(** * Exported properties

    Stated against the decision procedure, so discharging them
    requires the reflection lemma above rather than a [destruct] of
    the goal's own hypothesis. *)

Theorem transaction_value_conservation :
  forall t : Transaction,
    check_value_flow t = true ->
    inputs_total t >= outputs_total t + Fee t.
Proof.
  intros t H. apply check_value_flow_reflects in H.
  destruct H as [_ Hb]. exact Hb.
Qed.

Theorem transaction_no_negative_value :
  forall t : Transaction,
    check_value_flow t = true ->
    values_nonneg t.
Proof.
  intros t H. apply check_value_flow_reflects in H.
  destruct H as [Hn _]. exact Hn.
Qed.

Theorem transaction_fee_separation :
  forall t : Transaction,
    check_value_flow t = true ->
    0 <= Fee t /\ inputs_total t >= outputs_total t + Fee t.
Proof.
  intros t H. apply check_value_flow_reflects in H.
  destruct H as [[_ [_ Hf]] Hb]. split; [exact Hf | exact Hb].
Qed.

(** * Derived properties that are not conjuncts of the hypothesis

    These require the structural lemmas and are the reason the module
    is no longer a restatement of its own assumption. *)

Theorem transaction_totals_nonneg :
  forall t : Transaction,
    ValueFlow t ->
    0 <= inputs_total t /\ 0 <= outputs_total t.
Proof.
  intros t [[Hins [Houts _]] _].
  unfold inputs_total, outputs_total.
  split; [apply in_sum_nonneg, Hins | apply out_sum_nonneg, Houts].
Qed.

(** No individual output can exceed the transaction's total inputs.
    Needs induction over the output list; it does not follow from
    destructuring [ValueFlow]. *)
Theorem transaction_single_output_bounded :
  forall (t : Transaction) (o : Output),
    ValueFlow t ->
    In o (tx_outs t) ->
    out_value o <= inputs_total t.
Proof.
  intros t o Hv Hin.
  destruct Hv as [[_ [Houts Hfee]] Hbound].
  assert (Hle : out_value o <= outputs_total t)
    by (unfold outputs_total; apply out_sum_member_le; assumption).
  unfold outputs_bounded_by_inputs in Hbound. lia.
Qed.

(** The fee is bounded by the inputs, so fees cannot mint value. *)
Theorem transaction_fee_bounded_by_inputs :
  forall t : Transaction,
    ValueFlow t ->
    Fee t <= inputs_total t.
Proof.
  intros t Hv.
  destruct Hv as [[_ [Houts _]] Hbound].
  assert (Hout : 0 <= outputs_total t)
    by (unfold outputs_total; apply out_sum_nonneg, Houts).
  unfold outputs_bounded_by_inputs in Hbound. lia.
Qed.

(** An empty transaction cannot carry a positive fee. *)
Theorem empty_transaction_has_no_fee :
  forall t : Transaction,
    ValueFlow t ->
    tx_ins t = nil ->
    Fee t = 0.
Proof.
  intros t Hv Hnil.
  pose proof (transaction_fee_bounded_by_inputs t Hv) as Hfb.
  destruct Hv as [[_ [_ Hfee]] _].
  unfold inputs_total in Hfb. rewrite Hnil in Hfb. simpl in Hfb. lia.
Qed.

(** * Non-vacuity

    The decision procedure separates transactions; it is not constant. *)

Definition accepting_tx : Transaction :=
  {| tx_ins := [ {| in_value := 10 |} ];
     tx_outs := [ {| out_value := 7 |} ];
     tx_fee := 3 |}.

Definition overspending_tx : Transaction :=
  {| tx_ins := [ {| in_value := 10 |} ];
     tx_outs := [ {| out_value := 11 |} ];
     tx_fee := 0 |}.

Theorem check_value_flow_accepts_something :
  check_value_flow accepting_tx = true.
Proof. reflexivity. Qed.

Theorem check_value_flow_rejects_overspend :
  check_value_flow overspending_tx = false.
Proof. reflexivity. Qed.

Theorem check_value_flow_is_not_constant :
  (exists t, check_value_flow t = true) /\
  (exists t, check_value_flow t = false).
Proof.
  split.
  - exists accepting_tx. exact check_value_flow_accepts_something.
  - exists overspending_tx. exact check_value_flow_rejects_overspend.
Qed.
