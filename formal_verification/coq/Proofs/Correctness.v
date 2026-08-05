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
From Rigor Require Import Kernel.KernelEligibility.

(** PHASE-4A: [Verify] is no longer the stub [fun _ => true].
    It is the real validity predicate for a drawn winner: membership in the mint
    candidate set F together with the RES draw inequality at
    T = [Selection.RES_PROBABILITY_TARGET] (RFC-014 v1.1 §2.1; corrected from 2^240
    under SECURITY-001-A / A-6). *)
Definition Verify (b : Block) (B : list byte) (w : Tx) : Prop :=
  In w (build_F (txs b)) /\
  Z.lt (Sigma w (ConvergeRoot (build_F (txs b))) B (height b))
       (RES_PROBABILITY_TARGET * weight w).

(** Soundness: every winner produced by the executable [Execute] satisfies the
    real [Verify] predicate. Non-vacuous — it cannot hold for an arbitrary stub. *)
Theorem execute_implies_verify :
  forall b B w,
    Execute b 0%Z B = Some w -> Verify b B w.
Proof.
  intros b B w H. unfold Verify. split.
  - apply execute_winner_in_F with (B := B). exact H.
  - apply execute_winner_eligible with (B := B). exact H.
Qed.
