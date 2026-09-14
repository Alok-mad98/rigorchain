(** Phase5Equivalence/CandidateEquivalenceEvidenceModel.v
    Candidate Equivalence Evidence model (TASK-PROOF-025).

    Defines evidence categories, states, and readiness predicates without
    claiming equivalence. [cpp01_candidate_equivalence_ready] remains false.

    HONESTY BOUNDARY:
    - Coq evidence model only; no consensus / runtime / FVR changes.
    - [equivalence_claimed] remains false.
    - FVR-M draw prohibition remains active. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.

(** ================================================================
    Part A:  Evidence categories and states
    ================================================================ *)

Inductive EvidenceCategory :=
| CEE_Runtime
| CEE_GoldenVector
| CEE_Coq
| CEE_Invariant
| CEE_Audit
| CEE_DrawProhibited.

Inductive EvidenceState :=
| CEE_Missing
| CEE_Partial
| CEE_Sufficient.

Definition cee_state_sufficient (s : EvidenceState) : bool :=
  match s with CEE_Sufficient => true | _ => false end.

Definition cee_state_missing (s : EvidenceState) : bool :=
  match s with CEE_Missing => true | _ => false end.

(** ================================================================
    Part B:  Frozen baseline evidence table
    ================================================================ *)

Definition cee_baseline_runtime : EvidenceState := CEE_Missing.
Definition cee_baseline_golden_vector : EvidenceState := CEE_Missing.
Definition cee_baseline_coq : EvidenceState := CEE_Missing.
Definition cee_baseline_invariant : EvidenceState := CEE_Missing.
Definition cee_baseline_audit : EvidenceState := CEE_Missing.

Definition cee_draw_prohibition_active : bool := true.

Definition candidate_equivalence_evidence_state (cat : EvidenceCategory)
  : EvidenceState :=
  match cat with
  | CEE_Runtime => cee_baseline_runtime
  | CEE_GoldenVector => cee_baseline_golden_vector
  | CEE_Coq => cee_baseline_coq
  | CEE_Invariant => cee_baseline_invariant
  | CEE_Audit => cee_baseline_audit
  | CEE_DrawProhibited => CEE_Missing
  end.

(** ================================================================
    Part C:  Evidence readiness (parameterized + frozen)
    ================================================================ *)

Definition candidate_equivalence_evidence_ready_from
  (rt gv coq inv aud : EvidenceState) : bool :=
  cee_state_sufficient rt &&
  cee_state_sufficient gv &&
  cee_state_sufficient coq &&
  cee_state_sufficient inv &&
  cee_state_sufficient aud &&
  cee_draw_prohibition_active &&
  negb equivalence_claimed.

Definition candidate_equivalence_evidence_ready : bool :=
  candidate_equivalence_evidence_ready_from
    cee_baseline_runtime cee_baseline_golden_vector cee_baseline_coq
    cee_baseline_invariant cee_baseline_audit.

(** ================================================================
    Part D:  Master readiness predicate
    ================================================================ *)

Definition candidate_equivalence_ob02_ready_required
  (catalog : list DependencyState) : bool :=
  ds_ready (dependency_status PD4F_OB_02 catalog).

Definition candidate_equivalence_cpp01_not_closed_required
  (catalog : list DependencyState) : bool :=
  negb (ds_closed (dependency_status PD4F_CPP_01 catalog)).

Definition candidate_equivalence_ready (catalog : list DependencyState) : bool :=
  candidate_equivalence_ob02_ready_required catalog &&
  candidate_equivalence_evidence_ready &&
  candidate_equivalence_cpp01_not_closed_required catalog &&
  negb equivalence_claimed.

Definition candidate_equivalence_not_equivalence : Prop :=
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  equivalence_claimed = false.

Definition candidate_equivalence_requires_cpp01 : Prop :=
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.

(** ================================================================
    Part E:  Single-category insufficiency
    ================================================================ *)

Lemma cee_runtime_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Sufficient CEE_Missing CEE_Missing CEE_Missing CEE_Missing = false.
Proof. simpl. reflexivity. Qed.

Lemma cee_golden_vector_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Missing CEE_Sufficient CEE_Missing CEE_Missing CEE_Missing = false.
Proof. simpl. reflexivity. Qed.

Lemma cee_coq_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Missing CEE_Missing CEE_Sufficient CEE_Missing CEE_Missing = false.
Proof. simpl. reflexivity. Qed.

Lemma cee_all_sufficient_allows_evidence_ready :
  candidate_equivalence_evidence_ready_from
    CEE_Sufficient CEE_Sufficient CEE_Sufficient
    CEE_Sufficient CEE_Sufficient = true.
Proof. simpl. reflexivity. Qed.

Lemma cee_draw_prohibited_never_sufficient :
  cee_state_sufficient (candidate_equivalence_evidence_state CEE_DrawProhibited) = false.
Proof. simpl. reflexivity. Qed.

(** ================================================================
    Part F:  Readiness dependency theorems
    ================================================================ *)

Lemma candidate_equivalence_ready_implies_ob02_ready :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  candidate_equivalence_ob02_ready_required catalog = true.
Proof.
  intros catalog H.
  unfold candidate_equivalence_ready in H.
  destruct (candidate_equivalence_ob02_ready_required catalog) eqn:E;
    [|discriminate H].
  reflexivity.
Qed.

Theorem candidate_equivalence_ready_requires_ob02 :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_ready (dependency_status PD4F_OB_02 catalog) = true.
Proof.
  intros catalog H.
  apply candidate_equivalence_ready_implies_ob02_ready in H. exact H.
Qed.

Lemma candidate_equivalence_ready_implies_cpp01_not_closed :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  candidate_equivalence_cpp01_not_closed_required catalog = true.
Proof.
  intros catalog H.
  unfold candidate_equivalence_ready in H.
  destruct (candidate_equivalence_ob02_ready_required catalog) eqn:E;
    [|discriminate H].
  destruct candidate_equivalence_evidence_ready eqn:Er; [|discriminate H].
  destruct (candidate_equivalence_cpp01_not_closed_required catalog) eqn:Ec;
    [|discriminate H].
  reflexivity.
Qed.

Theorem candidate_equivalence_ready_forbids_cpp01_closed :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
Proof.
  intros catalog H.
  apply candidate_equivalence_ready_implies_cpp01_not_closed in H.
  unfold candidate_equivalence_cpp01_not_closed_required in H.
  destruct (ds_closed (dependency_status PD4F_CPP_01 catalog)) eqn:Ec;
    [|reflexivity].
  discriminate H.
Qed.

Theorem candidate_equivalence_not_equivalence_holds :
  candidate_equivalence_not_equivalence.
Proof.
  intros catalog H.
  unfold candidate_equivalence_ready in H.
  destruct (candidate_equivalence_ob02_ready_required catalog) eqn:E;
    [|discriminate H].
  destruct candidate_equivalence_evidence_ready eqn:Er; [|discriminate H].
  destruct (candidate_equivalence_cpp01_not_closed_required catalog) eqn:Ec;
    [|discriminate H].
  destruct equivalence_claimed eqn:Eq; [|reflexivity].
  discriminate H.
Qed.

Theorem candidate_equivalence_requires_cpp01_holds :
  candidate_equivalence_requires_cpp01.
Proof.
  intros catalog H.
  apply candidate_equivalence_ready_forbids_cpp01_closed. exact H.
Qed.

Lemma candidate_equivalence_ready_never_implies_equivalence :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  equivalence_claimed = false.
Proof.
  intros catalog H.
  exact (candidate_equivalence_not_equivalence_holds catalog H).
Qed.

(** ================================================================
    Part G:  Baseline state (evidence not ready)
    ================================================================ *)

Lemma cee_baseline_runtime_missing :
  cee_state_missing cee_baseline_runtime = true.
Proof. reflexivity. Qed.

Lemma cee_baseline_golden_vector_missing :
  cee_state_missing cee_baseline_golden_vector = true.
Proof. reflexivity. Qed.

Lemma cee_baseline_coq_missing :
  cee_state_missing cee_baseline_coq = true.
Proof. reflexivity. Qed.

Lemma cee_baseline_invariant_missing :
  cee_state_missing cee_baseline_invariant = true.
Proof. reflexivity. Qed.

Lemma cee_baseline_audit_missing :
  cee_state_missing cee_baseline_audit = true.
Proof. reflexivity. Qed.

Lemma candidate_equivalence_evidence_ready_baseline_false :
  candidate_equivalence_evidence_ready = false.
Proof.
  unfold candidate_equivalence_evidence_ready,
         candidate_equivalence_evidence_ready_from,
         cee_baseline_runtime.
  simpl. reflexivity.
Qed.

Theorem candidate_equivalence_baseline_false :
  candidate_equivalence_ready pd4f_ob02_dependency_catalog = false /\
  candidate_equivalence_evidence_ready = false /\
  equivalence_claimed = false.
Proof.
  split; [| split].
  - unfold candidate_equivalence_ready.
    rewrite candidate_equivalence_evidence_ready_baseline_false.
    simpl. reflexivity.
  - exact candidate_equivalence_evidence_ready_baseline_false.
  - exact equivalence_not_claimed.
Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [candidate_equivalence_evidence_model_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
Definition candidate_equivalence_evidence_model_established : Prop := True.

Lemma candidate_equivalence_evidence_model_established_holds :
  candidate_equivalence_evidence_model_established.
Proof. exact I. Qed.
