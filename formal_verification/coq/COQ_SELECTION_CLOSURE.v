(** PHASE-4: closed deterministic selection (σ argmin + txid memcmp tie-break). *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Sigma.
Require Import Selection.

Export Selection.

(** Winner choice is a pure function of inputs. *)
Lemma pick_better_tx_deterministic :
  forall x y C B h x' y' C' B' h',
    x = x' -> y = y' -> C = C' -> B = B' -> h = h' ->
    pick_better_tx x y C B h =
    pick_better_tx x' y' C' B' h'.
Proof.
  intros. subst. reflexivity.
Qed.

Theorem select_winner_deterministic :
  forall l C B h w1 w2,
    select_winner l C B h = Some w1 ->
    select_winner l C B h = Some w2 ->
    w1 = w2.
Proof.
  intros l C B h w1 w2 H1 H2.
  congruence.
Qed.

(** Tie-break uses byte lexicographic order on [tx_id_bytes]. *)
Lemma pick_better_tx_tie_uses_txid :
  forall x y C B h,
    sigma_product_lt x y C B h = false ->
    sigma_product_lt y x C B h = false ->
    pick_better_tx x y C B h =
    if bytes_ltb (tx_id_bytes x) (tx_id_bytes y) then x else y.
Proof.
  intros x y C B h Hlt Hgt.
  unfold pick_better_tx, sigma_product_lt in *.
  rewrite Hlt, Hgt.
  reflexivity.
Qed.
