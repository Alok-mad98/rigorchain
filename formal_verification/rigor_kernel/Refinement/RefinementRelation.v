(** Refinement relation: concrete model state vs abstract kernel.

    [refines] is an interface. It does not connect production code.

    NOT CLAIMED: C++ refinement, binary equivalence, CompCert.
*)
From RigorKernel Require Import Refinement.AbstractKernel.
From RigorKernel Require Import Refinement.ConcreteKernel.

(** Named observation. M24 flips [abs_ok] via [negb]. *)
Definition observe_ok (c : ConcreteState) (a : AbstractState) : Prop :=
  conc_ok c = abs_ok a.

Definition refines (c : ConcreteState) (a : AbstractState) : Prop :=
  conc_supply c = abs_supply a /\ observe_ok c a.
