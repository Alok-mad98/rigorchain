(** RigorSurface.Supply.ORUModel — compile-surface slice. No theorems. *)
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Inductive input_domain := DomORU | DomRES.

Record oru_tx := {
  tx_dom : input_domain;
  oru_in : Z;
  oru_out : Z;
  res_in : Z;
  res_out : Z
}.
