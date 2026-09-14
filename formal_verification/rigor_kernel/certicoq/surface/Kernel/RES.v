(** RigorSurface.Kernel.RES — [check_res]. No theorems.

    RFC-020 body mint: [negb coinbase], no [draw_ok].
    Do not replace this with [fragment_checker.c].
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Supply.Poles.
From RigorSurface Require Import Supply.RESTx.

Local Open Scope Z_scope.

Definition RESState := res_tx.

Definition check_res (s : RESState) : bool :=
  andb (negb (coinbase s))
  (andb (rt_mint_ok s)
  (andb (0 <? mint_amount s)
  (andb (mint_amount s <=? RES_MAX_SUPPLY)
  (andb (res_mint_authorized (rt_height s))
  (andb (0 <=? burn_oru s)
        (mint_amount s * res_destroy_pole_sq (rt_height s)
           <=? Z.shiftl (burn_oru s) RES_DESTROY_SHIFT)))))).
