(** A-9-C no-double-win: a given txid may win at most once across the draw window.

    C++ counterpart: [ConvergenceWindowTail.already_paid] and the exclusion in
    [FinalizeWindowedDrawSet] (core/src/consensus/convergence_logic.cpp).

    [Selection.v] already models the window WIDTH and the T/W target. This module
    models the missing half of A-9-C: candidates whose txid was paid by a RES mint
    inside the window are dropped from F before C and the winner are computed.

    Nothing here claims Coq = C++. These are theorems about the Coq model of the
    exclusion rule. Correspondence to the node is held by golden vectors. *)

From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import Lia.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Sorting.Permutation.
Import ListNotations.

Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Rigor.Sigma.
Require Import Rigor.Selection.
Require Import Rigor.OrderIndependence.

Open Scope Z_scope.

(** * 1. Membership in the paid-txid list *)

Definition already_paid_txid (paid : list (list byte)) (id : list byte) : bool :=
  existsb (fun p => bytes_eqb p id) paid.

Lemma already_paid_txid_true_iff :
  forall paid id,
    already_paid_txid paid id = true <->
    exists p, In p paid /\ bytes_eqb p id = true.
Proof.
  intros paid id. unfold already_paid_txid.
  rewrite existsb_exists.
  split; intros [p [Hin Heq]]; exists p; split; assumption.
Qed.

Lemma already_paid_txid_cons_self :
  forall paid id, already_paid_txid (id :: paid) id = true.
Proof.
  intros paid id. unfold already_paid_txid. simpl.
  rewrite bytes_eqb_refl. reflexivity.
Qed.

Lemma already_paid_txid_cons_other :
  forall paid id id',
    already_paid_txid paid id = true ->
    already_paid_txid (id' :: paid) id = true.
Proof.
  intros paid id id' H. unfold already_paid_txid in *. simpl.
  destruct (bytes_eqb id' id); [reflexivity| exact H].
Qed.

Lemma already_paid_txid_perm :
  forall paid paid' id,
    Permutation paid paid' ->
    already_paid_txid paid id = already_paid_txid paid' id.
Proof.
  intros paid paid' id Hperm. unfold already_paid_txid.
  induction Hperm; simpl.
  - reflexivity.
  - rewrite IHHperm. reflexivity.
  - destruct (bytes_eqb x id), (bytes_eqb y id); reflexivity.
  - congruence.
Qed.

(** * 2. Exclusion *)

Definition exclude_already_paid (candidates : list Tx) (paid : list (list byte)) : list Tx :=
  filter (fun t => negb (already_paid_txid paid (tx_id_bytes t))) candidates.

Lemma filter_ext_eq :
  forall (A : Type) (f g : A -> bool) (l : list A),
    (forall x, f x = g x) -> filter f l = filter g l.
Proof.
  intros A f g l H. induction l as [|x xs IH]; simpl; [reflexivity|].
  rewrite H, IH. reflexivity.
Qed.

Lemma exclude_already_paid_in :
  forall cs paid t,
    In t (exclude_already_paid cs paid) <->
    In t cs /\ already_paid_txid paid (tx_id_bytes t) = false.
Proof.
  intros cs paid t. unfold exclude_already_paid.
  rewrite filter_In, negb_true_iff. reflexivity.
Qed.

Theorem exclude_already_paid_drops_paid :
  forall cs paid t,
    In t (exclude_already_paid cs paid) ->
    already_paid_txid paid (tx_id_bytes t) = false.
Proof.
  intros cs paid t H. apply exclude_already_paid_in in H. apply H.
Qed.

Theorem exclude_already_paid_preserves_unpaid :
  forall cs paid t,
    In t cs ->
    already_paid_txid paid (tx_id_bytes t) = false ->
    In t (exclude_already_paid cs paid).
Proof.
  intros cs paid t Hin Hnp. apply exclude_already_paid_in. split; assumption.
Qed.

Lemma exclude_already_paid_perm_paid :
  forall cs paid paid',
    Permutation paid paid' ->
    exclude_already_paid cs paid = exclude_already_paid cs paid'.
Proof.
  intros cs paid paid' Hperm. unfold exclude_already_paid.
  apply filter_ext_eq. intros t.
  rewrite (already_paid_txid_perm paid paid' (tx_id_bytes t) Hperm).
  reflexivity.
Qed.

Lemma filter_permutation :
  forall (A : Type) (f : A -> bool) l1 l2,
    Permutation l1 l2 -> Permutation (filter f l1) (filter f l2).
Proof.
  intros A f l1 l2 Hperm. induction Hperm; simpl.
  - constructor.
  - destruct (f x); [constructor; assumption | assumption].
  - destruct (f x) eqn:Hx; destruct (f y) eqn:Hy; simpl.
    + apply perm_swap.
    + constructor. apply Permutation_refl.
    + constructor. apply Permutation_refl.
    + apply Permutation_refl.
  - eapply Permutation_trans; eassumption.
Qed.

Theorem exclude_already_paid_perm_candidates :
  forall cs cs' paid,
    Permutation cs cs' ->
    Permutation (exclude_already_paid cs paid) (exclude_already_paid cs' paid).
Proof.
  intros cs cs' paid H. unfold exclude_already_paid.
  apply filter_permutation. exact H.
Qed.

Lemma in_map_filter_in_map :
  forall (A B : Type) (f : A -> bool) (g : A -> B) l y,
    In y (map g (filter f l)) -> In y (map g l).
Proof.
  intros A B f g l y H.
  rewrite in_map_iff in H. destruct H as [x [Heq Hin]].
  apply filter_In in Hin. destruct Hin as [Hin _].
  apply in_map_iff. exists x. split; assumption.
Qed.

Lemma distinct_txids_filter :
  forall (f : Tx -> bool) l,
    distinct_txids l -> distinct_txids (filter f l).
Proof.
  intros f l. unfold distinct_txids.
  induction l as [|x xs IH]; simpl; [intros H; exact H|].
  intros Hd. inversion Hd as [|? ? Hnin Hrest]; subst.
  destruct (f x) eqn:Hf; simpl.
  - constructor.
    + intros Hin. apply Hnin. eapply in_map_filter_in_map. exact Hin.
    + apply IH. exact Hrest.
  - apply IH. exact Hrest.
Qed.

Lemma positive_weights_filter :
  forall (f : Tx -> bool) l,
    positive_weights l -> positive_weights (filter f l).
Proof.
  intros f l Hw t Hin. apply Hw.
  apply filter_In in Hin. apply Hin.
Qed.

(** Exclusion is a function of the SET of candidates: two nodes that see the
    same unpaid candidates in different orders still draw from the same F. *)
Theorem exclude_already_paid_order_independent :
  forall cs cs' paid C B h,
    Permutation cs cs' ->
    positive_weights cs ->
    distinct_txids cs ->
    select_winner (exclude_already_paid cs paid) C B h =
    select_winner (exclude_already_paid cs' paid) C B h.
Proof.
  intros cs cs' paid C B h Hperm Hw Hd.
  apply select_winner_order_independent.
  - apply exclude_already_paid_perm_candidates. exact Hperm.
  - unfold exclude_already_paid. apply positive_weights_filter. exact Hw.
  - unfold exclude_already_paid. apply distinct_txids_filter. exact Hd.
Qed.

(** * 3. Window assembly *)

Record WindowBlock := mkWindowBlock {
  wb_candidates : list Tx;
  wb_paid : list (list byte)
}.

Definition window_paid (w : list WindowBlock) : list (list byte) :=
  flat_map wb_paid w.

Definition window_candidates (w : list WindowBlock) : list Tx :=
  flat_map wb_candidates w.

(** F is the window's candidates minus anything a mint in the same window paid.
    Mirrors FinalizeWindowedDrawSet: buried candidates, then the source block's
    own, with already_paid covering the full window including the source mint. *)
Definition assemble_F (w : list WindowBlock) : list Tx :=
  exclude_already_paid (window_candidates w) (window_paid w).

Theorem assemble_F_excludes_window_paid :
  forall w t,
    In t (assemble_F w) ->
    already_paid_txid (window_paid w) (tx_id_bytes t) = false.
Proof.
  intros w t H. unfold assemble_F in H.
  apply exclude_already_paid_drops_paid in H. exact H.
Qed.

(** An incomplete window is "no draw", never a silently shortened window.
    C++: ConvergenceWindowTail.complete == false returns an unset set. *)
Definition maybe_assemble_F (complete : bool) (w : list WindowBlock) : option (list Tx) :=
  if complete then Some (assemble_F w) else None.

Lemma incomplete_window_is_no_draw :
  forall w, maybe_assemble_F false w = None.
Proof. intros. reflexivity. Qed.

Lemma complete_window_assembles_F :
  forall w, maybe_assemble_F true w = Some (assemble_F w).
Proof. intros. reflexivity. Qed.

Lemma genesis_empty_window_is_complete :
  maybe_assemble_F true [] = Some [].
Proof. reflexivity. Qed.

(** * 4. Draw with exclusion *)

Definition eligible_filter (l : list Tx) (C B : list byte) (h T : Z) : list Tx :=
  filter (fun t => if Z_lt_dec (Sigma t C B h) (T * weight t) then true else false) l.

Definition draw_with_exclusion
    (candidates : list Tx) (paid : list (list byte))
    (C B : list byte) (h T : Z) : option Tx :=
  select_winner (eligible_filter (exclude_already_paid candidates paid) C B h T) C B h.

Theorem winner_not_already_paid :
  forall candidates paid C B h T w,
    draw_with_exclusion candidates paid C B h T = Some w ->
    already_paid_txid paid (tx_id_bytes w) = false.
Proof.
  intros candidates paid C B h T w Hsel.
  unfold draw_with_exclusion in Hsel.
  apply select_winner_in in Hsel.
  unfold eligible_filter in Hsel.
  apply filter_In in Hsel. destruct Hsel as [Hin _].
  apply exclude_already_paid_drops_paid in Hin. exact Hin.
Qed.

Theorem paid_candidate_cannot_win :
  forall candidates paid C B h T w,
    already_paid_txid paid (tx_id_bytes w) = true ->
    draw_with_exclusion candidates paid C B h T <> Some w.
Proof.
  intros candidates paid C B h T w Hpaid Hwin.
  apply winner_not_already_paid in Hwin. congruence.
Qed.

(** * 5. Sequential draws: a txid wins at most once *)

Definition count_wins (id : list byte) (outcomes : list (option Tx)) : nat :=
  length (filter (fun o =>
    match o with
    | Some w => bytes_eqb (tx_id_bytes w) id
    | None => false
    end) outcomes).

Lemma count_wins_nil : forall id, count_wins id [] = 0%nat.
Proof. intros. reflexivity. Qed.

Lemma count_wins_cons_none :
  forall id rest, count_wins id (None :: rest) = count_wins id rest.
Proof. intros. reflexivity. Qed.

Lemma count_wins_cons_some_eq :
  forall id w rest,
    bytes_eqb (tx_id_bytes w) id = true ->
    count_wins id (Some w :: rest) = S (count_wins id rest).
Proof.
  intros id w rest Heq. unfold count_wins. simpl. rewrite Heq. reflexivity.
Qed.

Lemma count_wins_cons_some_neq :
  forall id w rest,
    bytes_eqb (tx_id_bytes w) id = false ->
    count_wins id (Some w :: rest) = count_wins id rest.
Proof.
  intros id w rest Heq. unfold count_wins. simpl. rewrite Heq. reflexivity.
Qed.

(** A trace of window draws that records each winner into [already_paid] for
    every subsequent draw. This is the Coq image of A-9-C §3.5: a mint inside
    the window removes that txid from F for the rest of the window. *)
Inductive paid_closed_trace : list (list byte) -> list (option Tx) -> Prop :=
| pct_nil : forall paid, paid_closed_trace paid []
| pct_none : forall paid rest,
    paid_closed_trace paid rest ->
    paid_closed_trace paid (None :: rest)
| pct_win : forall paid w rest,
    already_paid_txid paid (tx_id_bytes w) = false ->
    paid_closed_trace (tx_id_bytes w :: paid) rest ->
    paid_closed_trace paid (Some w :: rest).

Lemma draw_sequence_is_paid_closed :
  forall candidates paid C B h T w rest,
    draw_with_exclusion candidates paid C B h T = Some w ->
    paid_closed_trace (tx_id_bytes w :: paid) rest ->
    paid_closed_trace paid (Some w :: rest).
Proof.
  intros. apply pct_win.
  - eapply winner_not_already_paid. eassumption.
  - assumption.
Qed.

Theorem paid_closed_no_replay :
  forall paid outcomes id,
    paid_closed_trace paid outcomes ->
    already_paid_txid paid id = true ->
    count_wins id outcomes = 0%nat.
Proof.
  intros paid outcomes id Htr. induction Htr; intros Hpaid.
  - reflexivity.
  - rewrite count_wins_cons_none. apply IHHtr. exact Hpaid.
  - destruct (bytes_eqb (tx_id_bytes w) id) eqn:Heq.
    + apply bytes_eqb_eq in Heq.
      rewrite Heq in H. congruence.
    + rewrite count_wins_cons_some_neq; [|exact Heq].
      apply IHHtr. apply already_paid_txid_cons_other. exact Hpaid.
Qed.

(** Load-bearing A-9-C theorem: across any window trace that records each
    winner into already_paid, a given txid is selected at most once. *)
Theorem txid_wins_at_most_once :
  forall paid outcomes id,
    paid_closed_trace paid outcomes ->
    (count_wins id outcomes <= 1)%nat.
Proof.
  intros paid outcomes id Htr. induction Htr.
  - unfold count_wins. simpl. apply Nat.le_0_l.
  - rewrite count_wins_cons_none. exact IHHtr.
  - destruct (bytes_eqb (tx_id_bytes w) id) eqn:Heq.
    + rewrite count_wins_cons_some_eq; [|exact Heq].
      assert (count_wins id rest = 0%nat) as Hz.
      { eapply paid_closed_no_replay; [exact Htr|].
        unfold already_paid_txid. simpl. rewrite Heq. reflexivity. }
      rewrite Hz. apply le_n.
    + rewrite count_wins_cons_some_neq; [|exact Heq]. exact IHHtr.
Qed.

(** Executable step: resolve one draw, then push the winner (if any) into paid. *)
Definition draw_step
    (candidates : list Tx) (paid : list (list byte))
    (C B : list byte) (h T : Z)
  : option Tx * list (list byte) :=
  match draw_with_exclusion candidates paid C B h T with
  | None => (None, paid)
  | Some w => (Some w, tx_id_bytes w :: paid)
  end.

Lemma draw_step_extends_paid_closed :
  forall candidates paid C B h T w paid' rest,
    draw_step candidates paid C B h T = (Some w, paid') ->
    paid_closed_trace paid' rest ->
    paid_closed_trace paid (Some w :: rest).
Proof.
  intros candidates paid C B h T w paid' rest Hstep Hrest.
  unfold draw_step in Hstep.
  destruct (draw_with_exclusion candidates paid C B h T) as [w0|] eqn:Hdraw;
    [|discriminate].
  inversion Hstep; subst.
  apply pct_win.
  - eapply winner_not_already_paid. exact Hdraw.
  - exact Hrest.
Qed.
