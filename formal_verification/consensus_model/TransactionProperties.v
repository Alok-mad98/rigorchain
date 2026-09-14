(** Internal Transaction / validation-acceptance lemmas. Not C++.

    well_formed_transaction: a model transaction cannot be both
    input-invalid and validation-accepted.

    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
From ConsensusModel Require Import TransactionModel.
From ConsensusModel Require Import UTXOState.
From ConsensusModel Require Import ValidationResult.
Import ListNotations.

Local Open Scope Z_scope.

Definition output_amount_ok (o : TxOutput) : bool :=
  0 <=? amount o.

Definition tx_well_formed (tx : Transaction) : bool :=
  match outputs tx with
  | nil => false
  | _ => forallb output_amount_ok (outputs tx)
  end.

Definition input_present (u : UTXOState) (i : TxInput) : bool :=
  match lookup u (previous_outpoint i) with
  | Some _ => true
  | None => false
  end.

Definition inputs_exist (tx : Transaction) (u : UTXOState) : bool :=
  forallb (input_present u) (inputs tx).

Definition invalid_input (tx : Transaction) (u : UTXOState) : bool :=
  negb (inputs_exist tx u).

Definition script_stack_ok (tx : Transaction) : bool :=
  orb (Nat.eqb (List.length (witness tx)) 0)
      (Nat.eqb (List.length (witness tx)) (List.length (inputs tx))).

Definition amount_ok (tx : Transaction) : bool :=
  forallb output_amount_ok (outputs tx).

(** Model-level checker. Not CheckTransaction / ConnectBlock. *)
Definition validate_transaction (tx : Transaction) (u : UTXOState) : ValidationResult :=
  {| tx_valid := tx_well_formed tx;
     inputs_valid := inputs_exist tx u;
     script_valid := script_stack_ok tx;
     amount_valid := amount_ok tx |}.

Theorem well_formed_transaction :
  forall (tx : Transaction) (u : UTXOState),
    invalid_input tx u = true ->
    accepted_validation (validate_transaction tx u) ->
    False.
Proof.
  intros tx u Hinv Hacc.
  unfold invalid_input in Hinv.
  apply negb_true_iff in Hinv.
  unfold accepted_validation, validation_all_ok, validate_transaction in Hacc.
  simpl in Hacc.
  rewrite Hinv in Hacc.
  rewrite andb_false_l in Hacc.
  rewrite andb_false_r in Hacc.
  discriminate.
Qed.
