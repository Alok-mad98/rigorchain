(** Phase4FConsensus/ConsensusTraceExecution.v
    Trace execution over decoded golden vectors (PHASE-4F-001C).

    Runs the abstract Phase-4F machine on JSON-derived [TraceCase] lists.
    Golden equivalence remains deferred ([gtr_golden_match] = false). *)

From Coq Require Import List.
Import ListNotations.
From Coq Require Import ZArith.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusDrawProjection.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.ConsensusVectorRegistry.

(** Execute one trace case (machine replay only; golden flag stays false). *)
Definition execute_trace_case (c : TraceCase) : GoldenTraceResult :=
  golden_trace_result_of_case c.

Definition trace_cases_for_family (fam : VectorFamily) : list TraceCase :=
  match fam with
  | VF_PHASE3_CR => trace_cases_PHASE3_CR
  | VF_PHASE3_SIG => trace_cases_PHASE3_SIG
  | VF_PHASE3_WIN => trace_cases_PHASE3_WIN
  | VF_PHASE4A_DRAW => trace_cases_PHASE4A_DRAW
  | VF_PHASE4B_REPLAY => trace_cases_PHASE4B_REPLAY
  | VF_PHASE4D_SUPPLY => trace_cases_PHASE4D_SUPPLY
  | VF_PHASE4E_CONNECTBLOCK => trace_cases_PHASE4E_CONNECTBLOCK
  end.

Definition execute_vector_family (fam : VectorFamily) : list GoldenTraceResult :=
  map execute_trace_case (trace_cases_for_family fam).

Definition registry_families : list VectorFamily :=
  [ VF_PHASE3_CR; VF_PHASE3_SIG; VF_PHASE3_WIN; VF_PHASE4A_DRAW;
    VF_PHASE4B_REPLAY; VF_PHASE4D_SUPPLY; VF_PHASE4E_CONNECTBLOCK ].

Definition execute_registry : list GoldenTraceResult :=
  fold_right
    (fun fam acc => execute_vector_family fam ++ acc)
    nil
    registry_families.

Definition execute_registry_cases : list TraceCase :=
  all_registry_trace_cases.

Definition registry_execution_count : nat :=
  length execute_registry.

Lemma registry_execution_count_eq_generated :
  registry_execution_count = generated_trace_case_count.
Proof.
  unfold registry_execution_count, execute_registry, generated_trace_case_count.
  simpl.
  reflexivity.
Qed.

Lemma registry_execution_count_eq_total :
  registry_execution_count = registry_total_vector_count.
Proof.
  rewrite registry_execution_count_eq_generated.
  unfold registry_total_vector_count.
  reflexivity.
Qed.

Theorem execute_registry_length_sound :
  length execute_registry = registry_total_vector_count.
Proof.
  exact registry_execution_count_eq_total.
Qed.

(** 001D: golden match now derived from draw projection, not a constant. *)
Theorem execute_trace_case_match_is_projection :
  forall (c : TraceCase),
    (execute_trace_case c).(gtr_golden_match) = projection_golden_match c.
Proof.
  intros c. unfold execute_trace_case.
  rewrite golden_trace_result_match_is_meta.
  symmetry. apply projection_golden_match_eq_meta.
Qed.

(** Projection results over the full registry (for audit counting). *)
Definition registry_projection_results : list DrawProjectionResult :=
  map project_draw_result all_registry_trace_cases.

Definition registry_match_count : nat :=
  count_status is_match registry_projection_results.

Definition registry_mismatch_count : nat :=
  count_status is_mismatch registry_projection_results.

Definition registry_na_count : nat :=
  count_status is_not_applicable registry_projection_results.

(** 001D verified counts: 14 MATCH (7 WIN x {PHASE3_WIN, PHASE4A_DRAW}), 0 MISMATCH. *)
Lemma registry_match_count_value : registry_match_count = 14%nat.
Proof. reflexivity. Qed.

Lemma registry_mismatch_count_value : registry_mismatch_count = 0%nat.
Proof. reflexivity. Qed.

Lemma registry_na_count_value : registry_na_count = 59%nat.
Proof. reflexivity. Qed.

(** ----------------------------------------------------------------------
    PD-4F-OB-02 (OPEN): the full oracle bridge (Coq vs C++) is deferred beyond
    001D. The obligation — that every registry entry is projected and compared,
    leaving no N/A rows — is stated as a [Prop] definition and is NOT asserted. *)
Definition registry_oracle_bridge_obligation : Prop :=
  registry_na_count = 0%nat.

(** Historical name retained for doc cross-references. It previously read
    [Theorem execute_registry_oracle_bridge_placeholder : True. Admitted.] — a
    vacuous statement that could be miscited as evidence of a completed bridge.
    It now proves the opposite and checkable fact: the registry bridge is
    INCOMPLETE, because 59 of the 73 registry entries remain not-applicable. *)
Theorem execute_registry_oracle_bridge_placeholder :
  registry_na_count <> 0%nat.
Proof.
  rewrite registry_na_count_value. discriminate.
Qed.
