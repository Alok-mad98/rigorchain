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
Require Import Rigor.legacy_account_model.Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import ConvergeRoot.
Require Import Sigma.
Require Import Rigor.legacy_account_model.Selection.
Require Import Execution.
From Rigor Require Import Kernel.KernelSelection.
From Rigor Require Import Kernel.KernelRoot.
From Rigor Require Import Kernel.KernelEligibility.
From Rigor Require Import DrawCandidateRoot.

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

Theorem kernel_winner_in_draw_eligible :
  forall b B w,
    Execute b 0%Z B = Some w ->
    In w (eligible_candidates (build_F (txs b))
            (ConvergeRoot (build_F (txs b))) B (height b)).
Proof. apply execute_winner_in_draw_eligible. Qed.

Theorem kernel_draw_candidate_root_length :
  forall F C B h, length (draw_candidate_root F C B h) = 32%nat.
Proof. apply draw_candidate_root_of_F_length. Qed.

(** Full kernel chain: Execute winner ∈ hashed eligible set, DCR hashes that
    set, and σ of the winner uses [ConvergeRoot F] (not DCR). Does not amend
    [kernel_draw_sound]. Does not claim coinbase / Merkle / header bind
    ([DrawCandidateRoot] §9). *)
Theorem kernel_draw_e2e_aligned :
  forall b B w,
    Execute b 0%Z B = Some w ->
    In w (eligible_candidates (build_F (txs b))
            (ConvergeRoot (build_F (txs b))) B (height b)) /\
    draw_candidate_root (build_F (txs b))
      (ConvergeRoot (build_F (txs b))) B (height b)
      = DrawCandidateRoot
          (eligible_candidates (build_F (txs b))
             (ConvergeRoot (build_F (txs b))) B (height b)) /\
    Z.lt (Sigma w (ConvergeRoot (build_F (txs b))) B (height b))
         (RES_PROBABILITY_TARGET * weight w).
Proof. apply execute_e2e_aligned. Qed.

(** Determinism of the kernel: [Execute] is a function, hence at most one winner. *)
Theorem kernel_draw_deterministic :
  forall b B w1 w2,
    Execute b 0%Z B = Some w1 ->
    Execute b 0%Z B = Some w2 ->
    w1 = w2.
Proof.
  intros b B w1 w2 H1 H2. rewrite H1 in H2. injection H2 as H2. exact H2.
Qed.
