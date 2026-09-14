(** Model-level transaction and block validation. Not ConnectBlock.
    [validate_block] here is a Coq function. The Parameter of the same
    name in refinement/interfaces.v is FUTURE_REFINEMENT_BOUNDARY.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import BlockModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
From ConsensusModel Require Import TransactionProperties.
Import ListNotations.

Definition invalid_transaction (tx : Transaction) (u : UTXOState) : bool :=
  negb (validation_all_ok (validate_transaction tx u)).

Theorem valid_transaction_implies_amount_consistent :
  forall (tx : Transaction) (u : UTXOState),
    accepted_validation (validate_transaction tx u) ->
    amount_ok tx = true.
Proof.
  intros tx u Hacc.
  unfold accepted_validation, validation_all_ok, validate_transaction in Hacc.
  simpl in Hacc.
  apply andb_true_iff in Hacc.
  destruct Hacc as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [_ Ham].
  exact Ham.
Qed.

Theorem valid_transaction_implies_inputs_exist :
  forall (tx : Transaction) (u : UTXOState),
    accepted_validation (validate_transaction tx u) ->
    inputs_exist tx u = true.
Proof.
  intros tx u Hacc.
  unfold accepted_validation, validation_all_ok, validate_transaction in Hacc.
  simpl in Hacc.
  apply andb_true_iff in Hacc.
  destruct Hacc as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [Hin _].
  exact Hin.
Qed.

Theorem valid_transaction_implies_scripts_checked :
  forall (tx : Transaction) (u : UTXOState),
    accepted_validation (validate_transaction tx u) ->
    script_stack_ok tx = true.
Proof.
  intros tx u Hacc.
  unfold accepted_validation, validation_all_ok, validate_transaction in Hacc.
  simpl in Hacc.
  apply andb_true_iff in Hacc.
  destruct Hacc as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [Hsc _].
  exact Hsc.
Qed.

Theorem invalid_transaction_cannot_be_accepted :
  forall (tx : Transaction) (u : UTXOState),
    invalid_transaction tx u = true ->
    ~ accepted_validation (validate_transaction tx u).
Proof.
  intros tx u Hinv Hacc.
  unfold invalid_transaction in Hinv.
  apply negb_true_iff in Hinv.
  unfold accepted_validation in Hacc.
  congruence.
Qed.

Definition and_result (a b : ValidationResult) : ValidationResult :=
  {| tx_valid := andb (tx_valid a) (tx_valid b);
     inputs_valid := andb (inputs_valid a) (inputs_valid b);
     script_valid := andb (script_valid a) (script_valid b);
     amount_valid := andb (amount_valid a) (amount_valid b) |}.

Definition result_ok : ValidationResult :=
  {| tx_valid := true;
     inputs_valid := true;
     script_valid := true;
     amount_valid := true |}.

Lemma and_result_all_ok :
  forall (a b : ValidationResult),
    validation_all_ok (and_result a b) = true ->
    validation_all_ok a = true /\ validation_all_ok b = true.
Proof.
  intros a b H.
  unfold validation_all_ok, and_result in H.
  simpl in H.
  apply andb_true_iff in H.
  destruct H as [Htx Hrest].
  apply andb_true_iff in Htx.
  apply andb_true_iff in Hrest.
  destruct Htx as [Htxa Htxb].
  destruct Hrest as [Hin Hrest].
  apply andb_true_iff in Hin.
  apply andb_true_iff in Hrest.
  destruct Hin as [Hina Hinb].
  destruct Hrest as [Hsc Ham].
  apply andb_true_iff in Hsc.
  apply andb_true_iff in Ham.
  destruct Hsc as [Hsca Hscb].
  destruct Ham as [Hama Hamb].
  unfold validation_all_ok.
  split.
  - rewrite Htxa, Hina, Hsca, Hama. reflexivity.
  - rewrite Htxb, Hinb, Hscb, Hamb. reflexivity.
Qed.

Lemma and_result_fail_l :
  forall (a b : ValidationResult),
    validation_all_ok a = false ->
    validation_all_ok (and_result a b) = false.
Proof.
  intros a b Ha.
  destruct (validation_all_ok (and_result a b)) eqn:E.
  - apply and_result_all_ok in E.
    destruct E as [Ha' _].
    rewrite Ha in Ha'.
    discriminate.
  - reflexivity.
Qed.

Lemma and_result_fail_r :
  forall (a b : ValidationResult),
    validation_all_ok b = false ->
    validation_all_ok (and_result a b) = false.
Proof.
  intros a b Hb.
  destruct (validation_all_ok (and_result a b)) eqn:E.
  - apply and_result_all_ok in E.
    destruct E as [_ Hb'].
    rewrite Hb in Hb'.
    discriminate.
  - reflexivity.
Qed.

Fixpoint validate_txs (txs : list Transaction) (u : UTXOState) : ValidationResult :=
  match txs with
  | nil => result_ok
  | tx :: rest => and_result (validate_transaction tx u) (validate_txs rest u)
  end.

Definition validate_block (b : Block) (u : UTXOState) : ValidationResult :=
  validate_txs (transactions b) u.

Fixpoint apply_txs (txs : list Transaction) (u : UTXOState) : UTXOState :=
  match txs with
  | nil => u
  | tx :: rest =>
      apply_txs rest
        (fold_left (fun s i => remove s (previous_outpoint i)) (inputs tx) u)
  end.

Definition apply_block (b : Block) (u : UTXOState) : UTXOState :=
  if validation_all_ok (validate_block b u)
  then apply_txs (transactions b) u
  else u.

Lemma validate_txs_ok_forall :
  forall (txs : list Transaction) (u : UTXOState),
    validation_all_ok (validate_txs txs u) = true ->
    Forall (fun tx => accepted_validation (validate_transaction tx u)) txs.
Proof.
  induction txs as [|tx rest IH]; intros u Hok.
  - constructor.
  - simpl in Hok.
    apply and_result_all_ok in Hok.
    destruct Hok as [Htx Hrest].
    constructor.
    + unfold accepted_validation. exact Htx.
    + apply IH. exact Hrest.
Qed.

Theorem accepted_block_contains_only_valid_transactions :
  forall (b : Block) (u : UTXOState),
    accepted_validation (validate_block b u) ->
    Forall (fun tx => accepted_validation (validate_transaction tx u))
           (transactions b).
Proof.
  intros b u Hacc.
  unfold accepted_validation, validate_block in Hacc.
  apply validate_txs_ok_forall.
  exact Hacc.
Qed.

Lemma not_accepted_is_false :
  forall (r : ValidationResult),
    ~ accepted_validation r ->
    validation_all_ok r = false.
Proof.
  intros r H.
  unfold accepted_validation in H.
  destruct (validation_all_ok r) eqn:E.
  - exfalso. apply H. reflexivity.
  - reflexivity.
Qed.

Lemma validate_txs_reject_in :
  forall (txs : list Transaction) (u : UTXOState) (tx : Transaction),
    In tx txs ->
    validation_all_ok (validate_transaction tx u) = false ->
    validation_all_ok (validate_txs txs u) = false.
Proof.
  induction txs as [|tx0 rest IH]; intros u tx Hin Hfail.
  - simpl in Hin. contradiction.
  - simpl in Hin.
    destruct Hin as [Heq | Hinrest].
    + subst tx0. simpl. apply and_result_fail_l. exact Hfail.
    + simpl. apply and_result_fail_r. apply IH with (tx := tx); assumption.
Qed.

Theorem rejected_transaction_blocks_state_transition :
  forall (b : Block) (u : UTXOState) (tx : Transaction),
    In tx (transactions b) ->
    ~ accepted_validation (validate_transaction tx u) ->
    apply_block b u = u.
Proof.
  intros b u tx Hin Hrej.
  unfold apply_block, validate_block.
  apply not_accepted_is_false in Hrej.
  rewrite (validate_txs_reject_in (transactions b) u tx Hin Hrej).
  reflexivity.
Qed.

Theorem failed_validation_preserves_state :
  forall (b : Block) (u : UTXOState),
    validation_all_ok (validate_block b u) = false ->
    apply_block b u = u.
Proof.
  intros b u Hfail.
  unfold apply_block.
  rewrite Hfail.
  reflexivity.
Qed.
