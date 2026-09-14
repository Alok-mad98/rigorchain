(** RigorSurface.Supply.Poles — RFC-020 mint poles used by [check_res].

    Slice of [Supply/SupplySchedule.v]. No [res_destroy_cost_at_height] display destroy cost.
    No theorems.
*)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Definition RES_DESTROY_POLE : Z := Z.shiftl 1 25.
Definition RES_DESTROY_SHIFT : Z := 40.
Definition RES_MAX_SUPPLY : Z := 419430400000000.

Definition res_mint_authorized (h : Z) : bool := 0 <=? h.

Definition res_destroy_pole_sq (h : Z) : Z :=
  (RES_DESTROY_POLE + h) * (RES_DESTROY_POLE + h).
