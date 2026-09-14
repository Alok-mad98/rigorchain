(** RigorSurface.Supply.RESTx — [res_tx] slice.

    Lifted out of [proofs/RES_Issuance/RESIssuance.v]. No theorems.
*)
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Supply.ORUModel.

Local Open Scope Z_scope.

Record res_tx := {
  rt_coinbase : bool;
  rt_mint_ok : bool;
  rt_mint_amount : Z;
  rt_height : Z;
  rt_burn_oru : Z;
  rt_dom : input_domain;
  rt_res_in : Z;
  rt_res_out : Z;
  rt_oru_in : Z
}.

Definition coinbase (tx : res_tx) : bool := rt_coinbase tx.
Definition mint_amount (tx : res_tx) : Z := rt_mint_amount tx.
Definition burn_oru (tx : res_tx) : Z := rt_burn_oru tx.
