Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.
Require Import State.
Require Import Execution.
Require Import Selection.

(* RES Cap constant *)
Definition RES_CAP : Z := 21000000.

(* Supply bound property *)
Definition supply_bound_property (s : State) : Prop :=
  Z.le s.(RES_supply) RES_CAP.

(* Non-negative supply *)
Definition non_negative_supply (s : State) : Prop :=
  Z.ge s.(ORU_supply) 0 /\ Z.ge s.(RES_supply) 0.

(* Height monotonicity *)
Definition height_monotonic (b : Block) (s : State) : Prop :=
  Z.eq b.(height) (s.(current_height) + 1).

(* Valid block property *)
Definition valid_block (b : Block) (s : State) : Prop :=
  Z.gt b.(height) s.(current_height).
