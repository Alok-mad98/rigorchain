(** RigorSurface.Kernel.ORU — [check_oru]. No theorems.

    Keep the body inequalities; do not “simplify”.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From RigorSurface Require Import Supply.ORUModel.
From RigorSurface Require Import Supply.KernelTx.

Local Open Scope Z_scope.

Definition ORUState := kernel_tx.

Definition valid_oru_tx_b (t : oru_tx) : bool :=
  andb (0 <=? oru_in t)
  (andb (0 <=? oru_out t)
  (andb (0 <=? res_in t)
  (andb (0 <=? res_out t)
  (andb (oru_out t <=? oru_in t)
        (match tx_dom t with
         | DomORU => andb (res_in t =? 0) (res_out t =? 0)
         | DomRES => andb (0 <? oru_in t) (res_out t <=? res_in t)
         end))))).

Definition check_oru (s : ORUState) : bool := valid_oru_tx_b (to_oru s).
