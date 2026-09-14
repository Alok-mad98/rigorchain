(** Transition refinement skeleton. Both steps are Coq models.

    If [c] refines [a] and [concrete_step] fires, some [abstract_step]
    exists that restores [refines].

    NOT CLAIMED: production ConnectBlock, C++ refinement, CompCert.
*)
From Coq Require Import ZArith.ZArith.
From RigorKernel Require Import Refinement.AbstractKernel.
From RigorKernel Require Import Refinement.ConcreteKernel.
From RigorKernel Require Import Refinement.RefinementRelation.

Local Open Scope Z_scope.

Definition abstract_step (a a' : AbstractState) : Prop :=
  abs_ok a = true /\
  abs_supply a' = abs_supply a /\
  abs_ok a' = true.

(** Model-level concrete step. Not a production transition. *)
Definition concrete_step (c c' : ConcreteState) : Prop :=
  conc_ok c = true /\
  conc_supply c' = conc_supply c /\
  conc_ok c' = true.

Theorem refinement_preservation :
  forall (c : ConcreteState) (a : AbstractState) (c' : ConcreteState),
    refines c a ->
    concrete_step c c' ->
    exists a' : AbstractState,
      abstract_step a a' /\ refines c' a'.
Proof.
  intros c a c' [Hs Ho] [Hok [Hsup Hok']].
  unfold observe_ok in Ho.
  exists {| abs_supply := conc_supply c'; abs_ok := conc_ok c' |}.
  split.
  - unfold abstract_step. simpl.
    rewrite <- Ho, Hok, Hsup, Hs, Hok'.
    split; [reflexivity| split; reflexivity].
  - unfold refines, observe_ok. simpl.
    split; reflexivity.
Qed.
