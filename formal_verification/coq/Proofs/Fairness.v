Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import Sigma.
Require Import Rigor.legacy_account_model.Selection.

(** PHASE-4A: the prior [Pr := 1] constant made [weight_monotonic] vacuous.
    It is replaced by REAL deterministic selection-preference lemmas about the
    executable [pick_better_tx]. The probabilistic fairness statement is NOT a
    theorem about SHA-256; an idealized uniform-digest calculation lives in
    [Proofs/FairnessIdealized.v] and is explicitly not a claim about the hash. *)

(** Lower normalised sigma (cross-product) strictly wins the pairwise choice. *)
Theorem selection_prefers_lower_cross_product :
  forall x y C B h,
    sigma_product_lt x y C B h = true ->
    pick_better_tx x y C B h = x.
Proof.
  intros x y C B h Hlt. unfold pick_better_tx. rewrite Hlt. reflexivity.
Qed.

(** Pairwise selection is total: it returns one of the two candidates. *)
Theorem selection_total :
  forall x y C B h,
    pick_better_tx x y C B h = x \/ pick_better_tx x y C B h = y.
Proof.
  intros x y C B h. unfold pick_better_tx.
  destruct (sigma_product_lt x y C B h). { left; reflexivity. }
  destruct (sigma_product_lt y x C B h). { right; reflexivity. }
  destruct (bytes_ltb (tx_id_bytes x) (tx_id_bytes y)).
  - left; reflexivity.
  - right; reflexivity.
Qed.
