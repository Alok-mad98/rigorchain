Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.
Require Import State.
Require Import Execution.

(* State transition function *)
Definition apply_tx (s : State) (t : Tx) : State :=
  match asset_in t with
  | ORU =>
      {| ORU_supply := s.(ORU_supply) + t.(mint_cost);
         RES_supply := s.(RES_supply);
         current_height := s.(current_height);
         last_hash := s.(last_hash);
      |}
  | RES =>
      {| ORU_supply := s.(ORU_supply);
         RES_supply := s.(RES_supply) + t.(fee);
         current_height := s.(current_height);
         last_hash := s.(last_hash);
      |}
  end.

Definition apply_block (s : State) (b : Block) (T_ignored : Z) : option State :=
  match Execute b T_ignored b.(prev_block_bytes) with
  | None => Some s
  | Some w =>
      Some (apply_tx s w)
  end.
