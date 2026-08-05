(** Phase5Equivalence/CandidateEquivalenceRuntimeModel.v
    Candidate Equivalence Runtime Confirmation (TASK-PROOF-026).

    Maps PROOF-025 evidence categories to frozen runtime observations
    (FVR mirrors, golden-vector snapshots at h<=501) and confirms readiness
    predicates without claiming equivalence.

    HONESTY BOUNDARY:
    - Coq runtime confirmation only; no consensus / runtime / FVR changes.
    - [equivalence_claimed] remains false; [cpp01_candidate_equivalence_ready] false.
    - FVR-M draw prohibition remains active. *)

From Coq Require Import List Arith Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.
Require Import Rigor.Phase5Equivalence.CandidateEquivalenceEvidenceModel.

(** ================================================================
    Part A:  Runtime artifact mapping (frozen PROOF-015 h<=501)
    ================================================================ *)

Definition cee_runtime_fvr_mirror_layers : list string :=
  [ "FVR-B"%string; "FVR-C"%string; "FVR-F"%string;
    "FVR-G"%string; "FVR-H"%string; "FVR-I"%string; "FVR-N"%string ].

Definition cee_runtime_golden_vector_catalog : list string :=
  [ "GV-ORU-0001"%string; "GV-ORU-0002"%string; "GV-ORU-0003"%string;
    "GV-RES-0001"%string; "GV-RES-0002"%string; "GV-RES-0003"%string;
    "GV-DRAW-0001"%string; "GV-DRAW-0002"%string; "GV-DRAW-0003"%string;
    "GV-STATE-0001"%string; "GV-STATE-0002"%string; "GV-STATE-0003"%string ].

Definition cee_runtime_golden_vector_total : nat := 12%nat.

Definition cee_runtime_golden_vector_materialized_h501 : nat := 9%nat.

Definition cee_runtime_observation_height : nat := 501%nat.

Definition cee_runtime_harness_equivalence_claimed : bool := false.

Definition cee_runtime_harness_draw_prohibited : bool := true.

(** ================================================================
    Part B:  Runtime-observed category states (partial, not sufficient)
    ================================================================ *)

Definition cee_runtime_observed_runtime : EvidenceState := CEE_Partial.
Definition cee_runtime_observed_golden_vector : EvidenceState := CEE_Partial.
Definition cee_runtime_observed_coq : EvidenceState := CEE_Partial.
Definition cee_runtime_observed_invariant : EvidenceState := CEE_Missing.
Definition cee_runtime_observed_audit : EvidenceState := CEE_Partial.

Definition candidate_equivalence_runtime_category_state (cat : EvidenceCategory)
  : EvidenceState :=
  match cat with
  | CEE_Runtime => cee_runtime_observed_runtime
  | CEE_GoldenVector => cee_runtime_observed_golden_vector
  | CEE_Coq => cee_runtime_observed_coq
  | CEE_Invariant => cee_runtime_observed_invariant
  | CEE_Audit => cee_runtime_observed_audit
  | CEE_DrawProhibited => CEE_Missing
  end.

(** ================================================================
    Part C:  Runtime evidence readiness
    ================================================================ *)

Definition candidate_equivalence_runtime_evidence_ready : bool :=
  candidate_equivalence_evidence_ready_from
    cee_runtime_observed_runtime
    cee_runtime_observed_golden_vector
    cee_runtime_observed_coq
    cee_runtime_observed_invariant
    cee_runtime_observed_audit.

Definition candidate_equivalence_runtime_ob02_required
  (catalog : list DependencyState) : bool :=
  ds_ready (dependency_status PD4F_OB_02 catalog).

Definition candidate_equivalence_runtime_cpp01_not_closed
  (catalog : list DependencyState) : bool :=
  negb (ds_closed (dependency_status PD4F_CPP_01 catalog)).

Definition candidate_equivalence_runtime_ready (catalog : list DependencyState) : bool :=
  candidate_equivalence_runtime_ob02_required catalog &&
  candidate_equivalence_runtime_evidence_ready &&
  candidate_equivalence_runtime_cpp01_not_closed catalog &&
  cee_draw_prohibition_active &&
  negb equivalence_claimed.

Definition candidate_equivalence_runtime_not_equivalence : Prop :=
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  equivalence_claimed = false.

(** JSON audit export path (schema: formal_verification/schemas/
    candidate_equivalence_runtime_witness.schema.json). *)
Definition candidate_equivalence_runtime_witness_schema : string :=
  "formal_verification/schemas/candidate_equivalence_runtime_witness.schema.json"%string.

(** ================================================================
    Part D:  Runtime mapping confirmations
    ================================================================ *)

Lemma cee_runtime_fvr_mirror_layer_count :
  List.length cee_runtime_fvr_mirror_layers = 7%nat.
Proof. reflexivity. Qed.

Lemma cee_runtime_golden_vector_catalog_count :
  List.length cee_runtime_golden_vector_catalog = cee_runtime_golden_vector_total.
Proof. reflexivity. Qed.

Definition cee_runtime_materialized_le_total_bool : bool :=
  Nat.leb cee_runtime_golden_vector_materialized_h501 cee_runtime_golden_vector_total.

Lemma cee_runtime_materialized_le_total :
  cee_runtime_materialized_le_total_bool = true.
Proof. vm_compute. reflexivity. Qed.

Lemma cee_runtime_harness_equivalence_not_claimed :
  cee_runtime_harness_equivalence_claimed = false.
Proof. reflexivity. Qed.

Lemma cee_runtime_harness_draw_prohibited_true :
  cee_runtime_harness_draw_prohibited = true.
Proof. reflexivity. Qed.

Lemma candidate_equivalence_runtime_observed_not_sufficient :
  forall cat,
  cee_state_sufficient (candidate_equivalence_runtime_category_state cat) = false.
Proof.
  intro cat. destruct cat; simpl; reflexivity.
Qed.

Lemma candidate_equivalence_runtime_partial_confirms_not_ready :
  candidate_equivalence_runtime_evidence_ready = false.
Proof.
  unfold candidate_equivalence_runtime_evidence_ready,
         candidate_equivalence_evidence_ready_from,
         cee_runtime_observed_runtime.
  simpl. reflexivity.
Qed.

Lemma candidate_equivalence_runtime_draw_never_contributes :
  cee_state_sufficient
    (candidate_equivalence_runtime_category_state CEE_DrawProhibited) = false.
Proof. simpl. reflexivity. Qed.

(** ================================================================
    Part E:  Dependency theorems
    ================================================================ *)

Lemma candidate_equivalence_runtime_ready_implies_ob02 :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  candidate_equivalence_runtime_ob02_required catalog = true.
Proof.
  intros catalog H.
  unfold candidate_equivalence_runtime_ready in H.
  destruct (candidate_equivalence_runtime_ob02_required catalog) eqn:E;
    [|discriminate H].
  reflexivity.
Qed.

Theorem candidate_equivalence_runtime_ready_requires_ob02 :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  ds_ready (dependency_status PD4F_OB_02 catalog) = true.
Proof.
  intros catalog H.
  apply candidate_equivalence_runtime_ready_implies_ob02 in H. exact H.
Qed.

Lemma candidate_equivalence_runtime_ready_implies_cpp01_open :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  candidate_equivalence_runtime_cpp01_not_closed catalog = true.
Proof.
  intros catalog H.
  unfold candidate_equivalence_runtime_ready in H.
  destruct (candidate_equivalence_runtime_ob02_required catalog) eqn:E;
    [|discriminate H].
  destruct candidate_equivalence_runtime_evidence_ready eqn:Er; [|discriminate H].
  destruct (candidate_equivalence_runtime_cpp01_not_closed catalog) eqn:Ec;
    [|discriminate H].
  reflexivity.
Qed.

Theorem candidate_equivalence_runtime_ready_forbids_cpp01_closed :
  forall catalog,
  candidate_equivalence_runtime_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
Proof.
  intros catalog H.
  apply candidate_equivalence_runtime_ready_implies_cpp01_open in H.
  unfold candidate_equivalence_runtime_cpp01_not_closed in H.
  destruct (ds_closed (dependency_status PD4F_CPP_01 catalog)) eqn:Ec;
    [|reflexivity].
  discriminate H.
Qed.

Theorem candidate_equivalence_runtime_not_equivalence_holds :
  candidate_equivalence_runtime_not_equivalence.
Proof.
  intros catalog H.
  unfold candidate_equivalence_runtime_ready in H.
  destruct (candidate_equivalence_runtime_ob02_required catalog) eqn:E;
    [|discriminate H].
  destruct candidate_equivalence_runtime_evidence_ready eqn:Er; [|discriminate H].
  destruct (candidate_equivalence_runtime_cpp01_not_closed catalog) eqn:Ec;
    [|discriminate H].
  destruct cee_draw_prohibition_active eqn:Ed; [|discriminate H].
  destruct equivalence_claimed eqn:Eq; [|reflexivity].
  discriminate H.
Qed.

(** ================================================================
    Part F:  Baseline (runtime readiness false)
    ================================================================ *)

Theorem candidate_equivalence_runtime_baseline_false :
  candidate_equivalence_runtime_ready pd4f_ob02_dependency_catalog = false /\
  candidate_equivalence_runtime_evidence_ready = false /\
  cee_runtime_harness_equivalence_claimed = false /\
  equivalence_claimed = false.
Proof.
  split; [| split; [| split]].
  - unfold candidate_equivalence_runtime_ready.
    rewrite candidate_equivalence_runtime_partial_confirms_not_ready.
    simpl. reflexivity.
  - exact candidate_equivalence_runtime_partial_confirms_not_ready.
  - exact cee_runtime_harness_equivalence_not_claimed.
  - exact equivalence_not_claimed.
Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [candidate_equivalence_runtime_model_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
Definition candidate_equivalence_runtime_model_established : Prop := True.

Lemma candidate_equivalence_runtime_model_established_holds :
  candidate_equivalence_runtime_model_established.
Proof. exact I. Qed.
