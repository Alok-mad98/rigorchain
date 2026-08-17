(** Idealized fairness layer (post-verdict, optional).

    [Proofs/Fairness.v] proves only structural facts about [pick_better_tx].
    The probabilistic statement — a candidate burning f wins with probability
    proportional to f — needs a probability model the development does not
    have over SHA-256.

    This module states the RFC-014 calculation as arithmetic over Q, under an
    *idealized* uniform digest on [0, 2^256). It is NOT a theorem about
    SHA-256, grinding cost, or the C++ node. No axiom is introduced: the
    uniform assumption is a definition of the idealized measure, not a claim
    that σ is uniform. *)

From Coq Require Import ZArith.ZArith.
From Coq Require Import QArith.QArith.
From Coq Require Import QArith.Qfield.
From Coq Require Import Lia.

Require Import Rigor.Selection.

Local Open Scope Z_scope.

Definition two256 : Z := Z.shiftl 1 256.

(** P_ideal(eligible | weight f) = min(1, T·f / 2^256). *)
Definition idealized_eligible_prob (T f : Z) : Q :=
  if (T * f <=? 0)%Z then 0%Q
  else if (T * f <? two256)%Z then (inject_Z (T * f) / inject_Z two256)%Q
  else 1%Q.

Lemma two256_pos : 0 < two256.
Proof. unfold two256. vm_compute. reflexivity. Qed.

Lemma idealized_p_zero_nonpositive :
  forall T f, T * f <= 0 -> idealized_eligible_prob T f == 0%Q.
Proof.
  intros T f Hle. unfold idealized_eligible_prob.
  assert (Hb : (T * f <=? 0) = true) by (apply Z.leb_le; exact Hle).
  rewrite Hb. reflexivity.
Qed.

Lemma idealized_p_linear_below_saturation :
  forall T f,
    0 < T ->
    0 < f ->
    T * f < two256 ->
    idealized_eligible_prob T f == (inject_Z (T * f) / inject_Z two256)%Q.
Proof.
  intros T f HT Hf Hlt. unfold idealized_eligible_prob.
  assert (Hpos : (T * f <=? 0) = false).
  { apply Z.leb_gt. nia. }
  rewrite Hpos.
  assert (Hsat : (T * f <? two256) = true).
  { apply Z.ltb_lt. exact Hlt. }
  rewrite Hsat. reflexivity.
Qed.

Lemma idealized_p_saturated :
  forall T f,
    two256 <= T * f ->
    0 < T * f ->
    idealized_eligible_prob T f == 1%Q.
Proof.
  intros T f Hge Hpos. unfold idealized_eligible_prob.
  assert (Hleb : (T * f <=? 0) = false).
  { apply Z.leb_gt. exact Hpos. }
  rewrite Hleb.
  assert (Hltb : (T * f <? two256) = false).
  { apply Z.ltb_ge. exact Hge. }
  rewrite Hltb. reflexivity.
Qed.

Lemma inject_Z_mult_compat : forall a b, inject_Z (a * b) == (inject_Z a * inject_Z b)%Q.
Proof. intros. rewrite inject_Z_mult. reflexivity. Qed.

(** Below saturation, two positive weights have win-probabilities in the same
    ratio as the weights themselves. This is the RFC-014 fairness calculation
    for the idealized uniform digest; it is the statement the structural
    lemmas in Fairness.v cannot reach. *)
Theorem idealized_fairness_proportional :
  forall T f1 f2,
    0 < T ->
    0 < f1 ->
    0 < f2 ->
    T * f1 < two256 ->
    T * f2 < two256 ->
    (idealized_eligible_prob T f1 / idealized_eligible_prob T f2 ==
     inject_Z f1 / inject_Z f2)%Q.
Proof.
  intros T f1 f2 HT Hf1 Hf2 Hs1 Hs2.
  rewrite (idealized_p_linear_below_saturation T f1 HT Hf1 Hs1).
  rewrite (idealized_p_linear_below_saturation T f2 HT Hf2 Hs2).
  rewrite !inject_Z_mult_compat.
  assert (HTnz : ~ inject_Z T == 0%Q).
  { intro H. unfold Qeq, inject_Z in H. simpl in H. lia. }
  assert (Hf2nz : ~ inject_Z f2 == 0%Q).
  { intro H. unfold Qeq, inject_Z in H. simpl in H. lia. }
  assert (HNnz : ~ inject_Z two256 == 0%Q).
  { intro H. unfold Qeq, inject_Z, two256 in H. simpl in H. discriminate. }
  field. repeat split; assumption.
Qed.

(** The production target is below saturation at the minimum weight, so the
    idealized model is in the linear (not clipped) regime for every admissible
    f that does not itself saturate. *)
Lemma production_target_linear_at_min_weight :
  RES_PROBABILITY_TARGET * MIN_CONVERGENCE_AMOUNT < two256.
Proof. unfold two256. vm_compute. reflexivity. Qed.
