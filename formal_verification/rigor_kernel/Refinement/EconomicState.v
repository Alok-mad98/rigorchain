(** Economic refinement states. Coq models only. Not C++.

    NOT CLAIMED: production supply counters, compiler, binary.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Record EconomicAbstractState := {
  ea_oru : Z;
  ea_res : Z
}.

Record EconomicConcreteState := {
  ec_oru : Z;
  ec_res : Z
}.

(** Named so M28 can break ORU observation. *)
Definition observe_econ_oru
    (c : EconomicConcreteState) (a : EconomicAbstractState) : Prop :=
  ec_oru c = ea_oru a.

Definition economic_refines
    (c : EconomicConcreteState) (a : EconomicAbstractState) : Prop :=
  observe_econ_oru c a /\ ec_res c = ea_res a.
