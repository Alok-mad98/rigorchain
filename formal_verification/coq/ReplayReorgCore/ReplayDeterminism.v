(** ReplayReorgCore/ReplayDeterminism.v — replay determinism proof targets (PHASE-4B/4C). *)
From Coq Require Import List.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.StateTransition.
Require Import Rigor.ReplayReorgCore.ReorgRules.

(** Primary PHASE-4B target: identical valid block sequences yield identical final state. *)
Theorem Replay_Determinism :
  forall (s0 : ReplayState) (blocks : list BlockAbs)
         (s_final1 s_final2 : ReplayState),
    valid_block_sequence s0 blocks = true ->
    fold_apply_blocks s0 blocks = Some s_final1 ->
    fold_apply_blocks s0 blocks = Some s_final2 ->
    s_final1 = s_final2.
Proof.
  intros s0 blocks s1 s2 _seq H1 H2.
  congruence.
Qed.

(** Corollary: replay state equality on authoritative projection. *)
Theorem Replay_Determinism_projection :
  forall (s0 : ReplayState) (blocks : list BlockAbs)
         (s1 s2 : ReplayState),
    valid_block_sequence s0 blocks = true ->
    fold_apply_blocks s0 blocks = Some s1 ->
    fold_apply_blocks s0 blocks = Some s2 ->
    replay_state_eq s1 s2.
Proof.
  intros s0 blocks s1 s2 Hseq H1 H2.
  assert (Heq : s1 = s2) by (eapply Replay_Determinism; eauto).
  subst s2.
  repeat split; reflexivity.
Qed.

(** Single-step determinism (used in future inductive replay proofs). *)
Theorem apply_block_step_deterministic :
  forall (s : ReplayState) (b : BlockAbs) (r1 r2 : ReplayState),
    apply_block s b = Some r1 ->
    apply_block s b = Some r2 ->
    r1 = r2.
Proof.
  eauto using apply_block_deterministic.
Qed.

(** Fork resolution determinism (candidate list fixed). *)
Theorem fork_resolution_deterministic :
  forall (candidates : list ChainCandidate) (w1 w2 : ChainCandidate),
    select_best_fork candidates = Some w1 ->
    select_best_fork candidates = Some w2 ->
    w1 = w2.
Proof.
  eauto using select_best_fork_deterministic.
Qed.

(** Rollback determinism. *)
Theorem rollback_deterministic_theorem :
  forall (s : ReplayState) (k : nat) (r1 r2 : ReplayState),
    rollback s k = Some r1 ->
    rollback s k = Some r2 ->
    r1 = r2.
Proof.
  eauto using rollback_deterministic.
Qed.

(** Reorg then replay suffix reaches the same tip as replay-from-common-ancestor.

    Under [rollback s k = Some (cc_state c)], [reorg_to_candidate] exposes [cc_state c]
  and subsequent [fold_apply_blocks] is identical (PHASE-4C closure). *)
Theorem replay_reorg_commutes :
  forall (s : ReplayState) (k : nat) (c : ChainCandidate)
         (blocks_after : list BlockAbs) (s_end : ReplayState),
    rollback s k = Some (cc_state c) ->
    fold_apply_blocks (cc_state c) blocks_after = Some s_end ->
    exists s_end', reorg_to_candidate s k c = Some s_end' /\
      fold_apply_blocks s_end' blocks_after = Some s_end.
Proof.
  intros s k c blocks_after s_end Hrb Hfold.
  exists (cc_state c).
  split.
  - unfold reorg_to_candidate. rewrite Hrb. reflexivity.
  - exact Hfold.
Qed.

(** Inductive replay: valid structural sequences always fold successfully. *)
Theorem valid_block_sequence_fold_some :
  forall (s : ReplayState) (blocks : list BlockAbs),
    valid_block_sequence s blocks = true ->
    exists s', fold_apply_blocks s blocks = Some s'.
Proof.
  eauto using Rigor.ReplayReorgCore.StateTransition.valid_block_sequence_fold_some.
Qed.
