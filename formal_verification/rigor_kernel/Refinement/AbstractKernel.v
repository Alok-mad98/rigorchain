(** Abstract consensus-kernel state. Interface only.

    This is the specification-side state for a future refinement.
    It is not a C++ type and not a node.

    NOT CLAIMED: C++ refinement, binary equivalence, verified compiler.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Record AbstractState := {
  abs_supply : Z;
  abs_ok : bool
}.

Definition abstract_wf (a : AbstractState) : Prop :=
  0 <= abs_supply a.
