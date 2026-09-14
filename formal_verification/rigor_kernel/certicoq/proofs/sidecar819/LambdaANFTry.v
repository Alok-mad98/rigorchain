(** Phase 9 sidecar 8.19. Do not Require Compiler.pipeline
    (that file contains [Axiom print_Clight]).

    Attempt: load published LambdaANF lemmas and see whether a
    [check_oru] [exp] exists without the plugin pipeline.
    It does not. A toy [Ehalt] is well-scoped; that is not
    [check_oru].
*)
From Coq Require Import List PArith.BinPos.
From CertiCoq Require Import LambdaANF.cps LambdaANF.identifiers
  LambdaANF.Ensembles_util.
From Coq Require Import Sets.Ensembles.

Import ListNotations.

Definition toy_halt : exp := Ehalt 1%positive.

Definition toy_well_scoped : Prop :=
  unique_bindings toy_halt /\
  Disjoint _ (bound_var toy_halt) (occurs_free toy_halt).

Lemma toy_halt_unique : unique_bindings toy_halt.
Proof.
  unfold toy_halt. constructor.
Qed.

(* [check_oru] is a Gallina [bool] function. There is no
   published CertiCoq 0.9+8.19 term [exp_of_check_oru] that we
   can feed to [correct] / [correct_cc] without running the
   plugin pipeline (and that pipeline axiomatizes [print_Clight]). *)

Definition exp_of_check_oru_available : bool := false.

Theorem no_lambdaanf_exp_of_check_oru :
  exp_of_check_oru_available = false.
Proof. reflexivity. Qed.
