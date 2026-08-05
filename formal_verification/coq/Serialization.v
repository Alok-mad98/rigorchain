Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.

(* Serialization functions for consensus-critical byte encoding *)

Definition serialize_asset (a : Asset) : Z :=
  match a with
  | ORU => 0
  | RES => 1
  end.

Definition serialize_tx (t : Tx) : Z :=
  (tx_id t) + (serialize_asset (asset_in t)) + (fee t) + (mint_cost t).

(** The σ context of a draw is exactly the payout height and the parent block hash;
    [target_prev] is a witness field validated against the parent header and does not
    take part. *)
Definition serialize_block (b : Block) : Z :=
  b.(height) + b.(prev_hash).
