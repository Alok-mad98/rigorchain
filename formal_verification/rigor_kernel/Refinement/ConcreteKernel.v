(** Concrete kernel *model* state. Not production C++.

    A placeholder for a future implementation state. The fields are
    Coq-model data. They do not refine [src/] or the node.

    NOT CLAIMED: C++ refinement, ABI, memory model, compiler.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Record ConcreteState := {
  conc_supply : Z;
  conc_ok : bool
}.

Definition concrete_wf (c : ConcreteState) : Prop :=
  0 <= conc_supply c.
