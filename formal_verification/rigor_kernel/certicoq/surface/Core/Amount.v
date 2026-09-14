(** RigorSurface.Core.Amount — compile-surface slice. No theorems. *)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.

Local Open Scope Z_scope.

Definition amount := Z.
Definition COIN : amount := 100000000.
Definition MAX_MONEY : amount := 10000000000 * COIN.

Definition money_range_b (v : amount) : bool :=
  andb (0 <=? v) (v <=? MAX_MONEY).
