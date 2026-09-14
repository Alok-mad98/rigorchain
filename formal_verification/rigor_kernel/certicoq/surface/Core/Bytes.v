(** RigorSurface.Core.Bytes — compile-surface slice.

    Definitional copy of the [byte] / [split_at] helpers that
    [classify_script] needs. No theorems.
*)
From Coq Require Import Arith.Arith.
From Coq Require Import List.
Import ListNotations.

Definition byte := nat.

Fixpoint split_at (n : nat) (xs : list byte) : option (list byte * list byte) :=
  match n, xs with
  | O, _ => Some (nil, xs)
  | S n', x :: xs' =>
      match split_at n' xs' with
      | Some (pre, post) => Some (x :: pre, post)
      | None => None
      end
  | S _, nil => None
  end.
