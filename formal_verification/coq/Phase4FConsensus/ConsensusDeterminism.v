(** Phase4FConsensus/ConsensusDeterminism.v
    Determinism scaffolding for end-to-end consensus (PHASE-4F skeleton).

    Theorems marked ADMIT-PHASE4F are proof debt; simple corollaries may be Qed. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusTrace.

(** ADMIT-PHASE4F: single-step determinism (trivial for current scaffold; full proof later). *)
Theorem execute_transition_deterministic :
  forall (s : ConsensusState) (t : ConsensusTransition)
         (s1 s2 : ConsensusState),
    execute_transition s t = Some s1 ->
    execute_transition s t = Some s2 ->
    s1 = s2.
Proof.
  intros s t s1 s2 H1 H2. congruence.
Qed.

(** Trace-level determinism. [execute_trace] is a total function of its inputs,
    so two successful runs on identical inputs coincide. Discharged under
    SECURITY-001-F (was ADMIT-PHASE4F); [valid_trace_unique_final] and
    [consensus_replay_determinism] below previously inherited that admit
    silently despite being marked [Qed]. *)
Theorem execute_trace_deterministic :
  forall (s0 : ConsensusState) (trace : consensus_trace)
         (s1 s2 : ConsensusState),
    execute_trace s0 trace = Some s1 ->
    execute_trace s0 trace = Some s2 ->
    s1 = s2.
Proof.
  intros s0 trace s1 s2 H1 H2. congruence.
Qed.

(** Valid trace implies unique final state. *)
Theorem valid_trace_unique_final :
  forall (s0 : ConsensusState) (trace : consensus_trace)
         (s1 s2 : ConsensusState),
    valid_trace s0 trace s1 ->
    valid_trace s0 trace s2 ->
    s1 = s2.
Proof.
  unfold valid_trace. intros.
  eapply execute_trace_deterministic; eauto.
Qed.

(** End-to-end replay determinism target (abstract; refine when oracle-linked). *)
Theorem consensus_replay_determinism :
  forall (s0 : ConsensusState) (trace : consensus_trace)
         (s_final : ConsensusState),
    valid_trace s0 trace s_final ->
    forall s_alt,
      execute_trace s0 trace = Some s_alt ->
      s_alt = s_final.
Proof.
  intros s0 trace s_final Hvalid s_alt Halt.
  unfold valid_trace in Hvalid.
  eapply execute_trace_deterministic; eauto.
Qed.

(** ----------------------------------------------------------------------
    PD-4F-OB-02 (OPEN): cross-oracle agreement (Python / C++ / Coq).

    The obligation is stated below as a [Prop] *definition*. Nothing in this
    development asserts that it holds, and no theorem depends on it.
    ---------------------------------------------------------------------- *)
Definition oracle_agreement_obligation : Prop :=
  forall (external_oracle : ConsensusState -> consensus_trace -> option ConsensusState)
         (s0 : ConsensusState) (trace : consensus_trace),
    external_oracle s0 trace = execute_trace s0 trace.

(** Historical name retained for doc cross-references. It previously read
    [Theorem oracle_agreement_placeholder : ... True. Admitted.] — a vacuous
    statement that could be miscited as evidence of cross-implementation
    agreement. It now states the only thing actually provable without a wired
    oracle: agreement is CONDITIONAL on the oracle matching [execute_trace].
    [oracle_agreement_obligation] itself remains OPEN (PD-4F-OB-02). *)
Theorem oracle_agreement_placeholder :
  forall (external_oracle : ConsensusState -> consensus_trace -> option ConsensusState)
         (s0 : ConsensusState) (trace : consensus_trace) (s_final : ConsensusState),
    external_oracle s0 trace = execute_trace s0 trace ->
    valid_trace s0 trace s_final ->
    external_oracle s0 trace = Some s_final.
Proof.
  intros external_oracle s0 trace s_final Hagree Hvalid.
  unfold valid_trace in Hvalid. rewrite Hagree. exact Hvalid.
Qed.
