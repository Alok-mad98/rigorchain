Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Rigor.legacy_account_model.Selection.

Definition is_mint (t : Tx) : bool :=
  match asset_in t with
  | ORU => true
  | _ => false
  end.

Definition build_F (txs : list Tx) : list Tx :=
  filter is_mint txs.

Definition CONVERGENCE_DRAW_MIN_TXS_FOR_RES : Z := 1%Z.

(** [B_prev] is B_{h-1}, the hash of the block that committed the drawn set; [b.(height)]
    is the payout height. Under SECURITY-001-A8 the drawn set and its root come from that
    same parent, so the whole draw is fixed by one already-mined header. *)
Definition Execute (b : Block) (T_ignored : Z) (B_prev : list byte) : option Tx :=
  let _ := T_ignored in
  let F := build_F b.(txs) in
  let C := ConvergeRoot F in
  let B := B_prev in
  if Z.ltb (Z.of_nat (length F)) CONVERGENCE_DRAW_MIN_TXS_FOR_RES then
    None
  else
    let candidates :=
        filter (fun t =>
          if Z_lt_dec (Sigma t C B b.(height))
                 (RES_PROBABILITY_TARGET * weight t)
          then true else false) F
    in
    select_winner candidates C B b.(height).
