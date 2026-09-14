(** Economic conservation is preserved under model refinement.

    Concrete remains a Coq model. Not production C++.

    NOT CLAIMED: C++ refinement, binary equivalence, CompCert.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Economic.ORU.ORUDomain.
From RigorKernel Require Import Economic.ORU.ORUTransition.
From RigorKernel Require Import Economic.ORU.ORUSoundness.
From RigorKernel Require Import Refinement.EconomicState.

Local Open Scope Z_scope.

Definition abstract_econ_step
    (a a' : EconomicAbstractState) (f : ORUValueFlow) : Prop :=
  ea_oru a' = oru_stock (oru_apply {| oru_stock := ea_oru a |} f) /\
  ea_res a' = ea_res a.

Definition concrete_econ_step
    (c c' : EconomicConcreteState) (f : ORUValueFlow) : Prop :=
  ec_oru c' = oru_stock (oru_apply {| oru_stock := ec_oru c |} f) /\
  ec_res c' = ec_res c.

Theorem economic_refinement_preserves_conservation :
  forall (c : EconomicConcreteState) (a : EconomicAbstractState)
         (c' : EconomicConcreteState) (f : ORUValueFlow),
    economic_refines c a ->
    concrete_econ_step c c' f ->
    exists a' : EconomicAbstractState,
      abstract_econ_step a a' f /\
      economic_refines c' a' /\
      old_value {| oru_stock := ea_oru a |} + incoming_value f
        = outgoing_value f {| oru_stock := ea_oru a' |} + burned_value f.
Proof.
  intros c a c' f [Ho Hr] [Hc Hu].
  unfold observe_econ_oru in Ho.
  remember (oru_apply {| oru_stock := ea_oru a |} f) as post eqn:Hpost.
  exists {| ea_oru := oru_stock post; ea_res := ea_res a |}.
  split.
  - unfold abstract_econ_step. simpl. split.
    + rewrite Hpost. reflexivity.
    + reflexivity.
  - split.
    + unfold economic_refines, observe_econ_oru. simpl.
      split.
      * rewrite Hc, Ho, Hpost. reflexivity.
      * rewrite Hu. exact Hr.
    + subst post. apply oru_domain_conservation.
Qed.
