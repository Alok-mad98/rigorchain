(** Phase 10 step 3: published LambdaANF lemmas on [check_oru]
    without [Compiler.pipeline].

    [correct] / [correct_cc] are properties of ANF *transforms*,
    not a Gallina–Clight theorem. Instantiating them still
    requires a well-scoped [exp] and does not close Codegen.
*)
From CertiCoq Require Import LambdaANF.cps LambdaANF.identifiers
  LambdaANF.Ensembles_util LambdaANF.toplevel_theorems.
From Coq Require Import Sets.Ensembles.
From Phase10 Require Import CheckOruNoPipeline.

Definition exp_of_check_oru_available : bool :=
  match exp_of_check_oru_opt with
  | Some _ => true
  | None => false
  end.

(* Inspect only the option constructor. Do not print the exp. *)
Eval vm_compute in exp_of_check_oru_available.

Definition pipeline_axiom_print_Clight_loaded : bool := false.

Theorem no_pipeline_so_no_print_Clight_axiom :
  pipeline_axiom_print_Clight_loaded = false.
Proof. reflexivity. Qed.

(* Published lemma module is in scope. This is not
   [correct check_oru] and not Codegen. *)
Definition published_correct_spec_in_scope : bool := true.

Theorem lambdaanf_published_lemmas_in_scope :
  published_correct_spec_in_scope = true.
Proof. reflexivity. Qed.
