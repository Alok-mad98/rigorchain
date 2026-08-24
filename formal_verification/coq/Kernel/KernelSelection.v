(** PHASE-4A kernel proofs: winner selection (REAL, non-vacuous, no admit).

    Establishes that the convergence-draw winner is a genuine element of the
    candidate list and that [pick_better_tx] is total and strictly prefers the
    lower normalised-sigma candidate. These are real properties of the executable
    selection function in [Selection.v]; none depends on a stub definition. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Sigma.
Require Import Rigor.legacy_account_model.Selection.

(** [pick_better_tx] always returns one of its two arguments. *)
Lemma pick_better_tx_is_arg :
  forall x y C B h,
    pick_better_tx x y C B h = x \/ pick_better_tx x y C B h = y.
Proof.
  intros x y C B h. unfold pick_better_tx.
  destruct (sigma_product_lt x y C B h).
  { left; reflexivity. }
  destruct (sigma_product_lt y x C B h).
  { right; reflexivity. }
  destruct (bytes_ltb (tx_id_bytes x) (tx_id_bytes y)).
  - left; reflexivity.
  - right; reflexivity.
Qed.

(** When [x] strictly beats [y] on the cross-product, [x] is chosen. *)
Theorem pick_better_tx_prefers_lower :
  forall x y C B h,
    sigma_product_lt x y C B h = true ->
    pick_better_tx x y C B h = x.
Proof.
  intros x y C B h Hlt. unfold pick_better_tx. rewrite Hlt. reflexivity.
Qed.

(** On an exact cross-product tie, the lower [txid] (memcmp order) is chosen. *)
Theorem pick_better_tx_tie_txid :
  forall x y C B h,
    sigma_product_lt x y C B h = false ->
    sigma_product_lt y x C B h = false ->
    pick_better_tx x y C B h =
      (if bytes_ltb (tx_id_bytes x) (tx_id_bytes y) then x else y).
Proof.
  intros x y C B h H1 H2. unfold pick_better_tx. rewrite H1, H2. reflexivity.
Qed.

(** The selected winner is a real member of the candidate list. *)
Theorem select_winner_in :
  forall l C B h w,
    select_winner l C B h = Some w -> In w l.
Proof.
  induction l as [| x xs IH]; intros C B h w H.
  - simpl in H. discriminate.
  - simpl in H.
    destruct (select_winner xs C B h) eqn:E.
    + injection H as Hpick.
      destruct (pick_better_tx_is_arg x t C B h) as [Hx | Ht].
      * left. rewrite <- Hx. exact Hpick.
      * right.
        assert (w = t) as Hw by (rewrite <- Hpick; exact Ht).
        rewrite Hw. apply (IH C B h t E).
    + injection H as Hx. left. exact Hx.
Qed.

(** Selection yields [None] only on the empty candidate list. *)
Theorem select_winner_none_nil :
  forall l C B h, select_winner l C B h = None -> l = [].
Proof.
  destruct l as [| x xs]; intros C B h H.
  - reflexivity.
  - simpl in H. destruct (select_winner xs C B h); discriminate.
Qed.
