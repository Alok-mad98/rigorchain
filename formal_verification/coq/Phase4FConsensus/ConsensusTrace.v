(** Phase4FConsensus/ConsensusTrace.v
    Trace execution template (PHASE-4F skeleton).

    SCAFFOLDING: validity predicates and trace metadata. Executable semantics
    live in [ConsensusMachine.execute_trace]. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.

(** Number of trace steps (structural; no hidden iteration). *)
Definition trace_step_count (trace : consensus_trace) : nat :=
  length trace.

(** One valid step: [execute_transition] succeeds. *)
Inductive ValidTraceStep : ConsensusState -> ConsensusTransition -> ConsensusState -> Prop :=
  | VTS_execute :
      forall s t s',
        execute_transition s t = Some s' ->
        ValidTraceStep s t s'.

(** Full trace reaches [sf] from [s0] via [execute_trace]. *)
Definition valid_trace
  (s0 : ConsensusState) (trace : consensus_trace) (sf : ConsensusState) : Prop :=
  execute_trace s0 trace = Some sf.

(** Trace rejected (any step returns [None]). *)
Definition rejected_trace
  (s0 : ConsensusState) (trace : consensus_trace) : Prop :=
  execute_trace s0 trace = None.

(** Structural: empty trace is the identity on state. *)
Lemma empty_trace_identity :
  forall s, execute_trace s nil = Some s.
Proof.
  intros s. simpl. reflexivity.
Qed.

(** Trace append: executing [trace1] then [trace2] equals executing their
    concatenation. Discharged under SECURITY-001-F (was ADMIT-PHASE4F). *)
Theorem execute_trace_append :
  forall s trace1 trace2 s_mid s_final,
    execute_trace s trace1 = Some s_mid ->
    execute_trace s_mid trace2 = Some s_final ->
    execute_trace s (trace1 ++ trace2) = Some s_final.
Proof.
  intros s trace1. revert s.
  induction trace1 as [|t rest IH]; intros s trace2 s_mid s_final H1 H2.
  - simpl in H1. inversion H1. subst s_mid. simpl. exact H2.
  - simpl in H1 |- *.
    destruct (execute_transition s t) as [s'|] eqn:E; [|discriminate H1].
    eapply IH; eauto.
Qed.
