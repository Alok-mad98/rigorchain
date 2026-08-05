(** PHASE-4A kernel proofs: mint eligibility of the draw winner (REAL, no admit).

    Connects the full executable pipeline F -> C -> Sigma -> Winner: any winner
    returned by [Execute] is (1) a member of the mint candidate set F and
    (2) satisfies the RES draw inequality  Sigma_w < T * weight_w  with
    T = RES_PROBABILITY_TARGET (RFC-014 v1.1 §2.1; corrected from 2^240 under
    SECURITY-001-A / A-6). The [None] case is characterised too.

    The proofs below are symbolic in T and were unaffected by the A-6 correction; what
    the correction changed is whether clause (2) carries information at all. Under the
    old constant it was implied by the digest range itself — see
    [Selection.pre_a6_every_digest_was_eligible].
    Nothing here is vacuous; every fact is derived from the executable
    [Execution.Execute]. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Selection.
Require Import Execution.
From Rigor Require Import Kernel.KernelSelection.

(** The drawn winner is a genuine element of F = build_F (txs b). *)
Theorem execute_winner_in_F :
  forall b B w,
    Execute b 0%Z B = Some w ->
    In w (build_F (txs b)).
Proof.
  intros b B w H. unfold Execute in H. cbv zeta in H.
  destruct (Z.ltb (Z.of_nat (length (build_F (txs b))))
                  CONVERGENCE_DRAW_MIN_TXS_FOR_RES) eqn:Eg.
  - discriminate.
  - apply select_winner_in in H.
    apply filter_In in H. destruct H as [HF _]. exact HF.
Qed.

(** The drawn winner satisfies the RES draw inequality with T = RES_PROBABILITY_TARGET. *)
Theorem execute_winner_eligible :
  forall b B w,
    Execute b 0%Z B = Some w ->
    Z.lt (Sigma w (ConvergeRoot (build_F (txs b))) B (height b))
         (RES_PROBABILITY_TARGET * weight w).
Proof.
  intros b B w H. unfold Execute in H. cbv zeta in H.
  destruct (Z.ltb (Z.of_nat (length (build_F (txs b))))
                  CONVERGENCE_DRAW_MIN_TXS_FOR_RES) eqn:Eg.
  - discriminate.
  - apply select_winner_in in H.
    apply filter_In in H. destruct H as [_ Hpred].
    destruct (Z_lt_dec
                (Sigma w (ConvergeRoot (build_F (txs b))) B (height b))
                (RES_PROBABILITY_TARGET * weight w)) as [Hlt | Hnlt].
    + exact Hlt.
    + discriminate Hpred.
Qed.

(** No winner is drawn only if F is below the cardinality gate or no candidate
    is eligible. *)
Theorem execute_none_characterisation :
  forall b B,
    Execute b 0%Z B = None ->
    (Z.lt (Z.of_nat (length (build_F (txs b)))) CONVERGENCE_DRAW_MIN_TXS_FOR_RES)
    \/
    (filter (fun t =>
        if Z_lt_dec (Sigma t (ConvergeRoot (build_F (txs b))) B (height b))
                    (RES_PROBABILITY_TARGET * weight t)
        then true else false)
       (build_F (txs b)) = []).
Proof.
  intros b B H. unfold Execute in H. cbv zeta in H.
  destruct (Z.ltb (Z.of_nat (length (build_F (txs b))))
                  CONVERGENCE_DRAW_MIN_TXS_FOR_RES) eqn:Eg.
  - left. apply Z.ltb_lt. exact Eg.
  - right. apply select_winner_none_nil in H. exact H.
Qed.
