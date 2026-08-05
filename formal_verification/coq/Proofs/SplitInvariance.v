Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Lia.
Require Import Types.
Require Import Selection.

(** PHASE-4A: the prior [probability := 1] constant made [split_invariance]
    vacuous. It is replaced by a REAL weight-conservation lemma: splitting one
    fee (a + b) into two parts a, b does not change the total summed weight.
    This is the concrete anti-Sybil arithmetic invariant; the full probabilistic
    split-invariance is tracked as OPEN (see PROOF_DEBT_REGISTER.md). *)
Theorem split_preserves_total :
  forall (pre post : list Z) (a b : Z),
    fold_left Z.add (pre ++ (a + b)%Z :: post) 0%Z =
    fold_left Z.add (pre ++ a :: b :: post) 0%Z.
Proof.
  intros pre post a b.
  rewrite !fold_left_app. simpl.
  f_equal. lia.
Qed.
