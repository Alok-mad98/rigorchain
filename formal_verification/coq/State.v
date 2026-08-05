Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.

(* Consensus State *)
Record State := {
  ORU_supply : Z;
  RES_supply : Z;
  current_height : Z;
  last_hash : bytes32;
}.

(* Initial state *)
Definition init_state : State :=
  {| ORU_supply := 0%Z;
     RES_supply := 0%Z;
     current_height := 0%Z;
     last_hash := 0%Z;
  |}.
