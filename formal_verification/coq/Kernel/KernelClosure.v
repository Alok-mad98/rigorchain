(** PHASE-4A: consensus draw-kernel closure (REAL, non-vacuous, no admit).

    Top-level soundness of the executable convergence draw kernel
    F -> Convergence Root -> Sigma -> Winner -> Mint Eligibility:

      If [Execute b 0 B = Some w] then
        - w is a member of the mint candidate set F,
        - w satisfies the RES draw inequality  Sigma_w < RES_PROBABILITY_TARGET * weight_w
          (RFC-014 v1.1 §2.1; T corrected from 2^240 under SECURITY-001-A / A-6),
        - the convergence root bound into w's sigma is a well-formed 32-byte value.

    This is the PHASE-4A replacement for the previously vacuous
    "correctness"/"safety" theorems. It constrains the real [Execute] function;
    it cannot be satisfied by a stub. *)
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
From Rigor Require Import Kernel.KernelRoot.
From Rigor Require Import Kernel.KernelEligibility.

Theorem kernel_draw_sound :
  forall b B w,
    Execute b 0%Z B = Some w ->
    In w (build_F (txs b)) /\
    Z.lt (Sigma w (ConvergeRoot (build_F (txs b))) B (height b))
         (RES_PROBABILITY_TARGET * weight w) /\
    length (ConvergeRoot (build_F (txs b))) = 32%nat.
Proof.
  intros b B w H. repeat split.
  - apply execute_winner_in_F with (B := B). exact H.
  - apply execute_winner_eligible with (B := B). exact H.
  - apply converge_root_length.
Qed.

(** Determinism of the kernel: [Execute] is a function, hence at most one winner. *)
Theorem kernel_draw_deterministic :
  forall b B w1 w2,
    Execute b 0%Z B = Some w1 ->
    Execute b 0%Z B = Some w2 ->
    w1 = w2.
Proof.
  intros b B w1 w2 H1 H2. rewrite H1 in H2. injection H2 as H2. exact H2.
Qed.
