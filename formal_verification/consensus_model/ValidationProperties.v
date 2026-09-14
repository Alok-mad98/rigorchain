(** Internal ValidationResult lemmas. Not C++ refinement.
    CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
*)
From Coq Require Import Bool.Bool.
From ConsensusModel Require Import ValidationResult.

Theorem accepted_validation :
  forall (r : ValidationResult),
    validation_all_ok r = true ->
    inputs_valid r = true /\
    script_valid r = true /\
    amount_valid r = true.
Proof.
  intros r Hacc.
  unfold validation_all_ok in Hacc.
  apply andb_true_iff in Hacc.
  destruct Hacc as [_ Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [Hin Hrest].
  apply andb_true_iff in Hrest.
  destruct Hrest as [Hsc Ham].
  repeat split; assumption.
Qed.
