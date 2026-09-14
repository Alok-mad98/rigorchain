(** Transaction semantic layer. Coq kernel model only.

    NOT CLAIMED: C++ tx parser, signatures, ConnectBlock, node validation.
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.

Local Open Scope Z_scope.

Record Input := { in_value : Z }.

Record Output := { out_value : Z }.

Record Transaction := {
  tx_ins : list Input;
  tx_outs : list Output;
  tx_fee : Z
}.

Definition Fee (t : Transaction) : Z := tx_fee t.

Fixpoint in_sum (xs : list Input) : Z :=
  match xs with
  | nil => 0
  | i :: rest => in_value i + in_sum rest
  end.

Fixpoint out_sum (xs : list Output) : Z :=
  match xs with
  | nil => 0
  | o :: rest => out_value o + out_sum rest
  end.

Definition inputs_total (t : Transaction) : Z := in_sum (tx_ins t).

Definition outputs_total (t : Transaction) : Z := out_sum (tx_outs t).
