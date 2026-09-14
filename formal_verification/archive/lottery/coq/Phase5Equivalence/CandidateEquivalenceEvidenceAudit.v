(** Phase5Equivalence/CandidateEquivalenceEvidenceAudit.v
    Audit layer for Candidate Equivalence Evidence model (TASK-PROOF-025).

    Re-exports evidence predicates and verifies the frozen baseline:
    all categories missing, draw prohibition active, readiness false.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.
Require Import Rigor.Phase5Equivalence.PD4FCPP01ClosureModel.
Require Import Rigor.Phase5Equivalence.CandidateEquivalenceEvidenceModel.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition audit_cee_draw_prohibition_active : bool := cee_draw_prohibition_active.

Definition audit_candidate_equivalence_evidence_ready : bool :=
  candidate_equivalence_evidence_ready.

Definition audit_candidate_equivalence_ready : bool :=
  candidate_equivalence_ready pd4f_ob02_dependency_catalog.

Definition audit_cpp01_candidate_gate : bool := cpp01_candidate_equivalence_ready.

(** ---- Category baseline re-exports ---- *)

Lemma audit_cee_runtime_baseline_missing :
  cee_state_missing (candidate_equivalence_evidence_state CEE_Runtime) = true.
Proof. exact cee_baseline_runtime_missing. Qed.

Lemma audit_cee_golden_vector_baseline_missing :
  cee_state_missing (candidate_equivalence_evidence_state CEE_GoldenVector) = true.
Proof. exact cee_baseline_golden_vector_missing. Qed.

Lemma audit_cee_coq_baseline_missing :
  cee_state_missing (candidate_equivalence_evidence_state CEE_Coq) = true.
Proof. exact cee_baseline_coq_missing. Qed.

Lemma audit_cee_draw_prohibited_never_sufficient :
  cee_state_sufficient (candidate_equivalence_evidence_state CEE_DrawProhibited) = false.
Proof. exact cee_draw_prohibited_never_sufficient. Qed.

(** ---- Insufficiency re-exports ---- *)

Lemma audit_cee_runtime_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Sufficient CEE_Missing CEE_Missing CEE_Missing CEE_Missing = false.
Proof. exact cee_runtime_alone_insufficient. Qed.

Lemma audit_cee_golden_vector_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Missing CEE_Sufficient CEE_Missing CEE_Missing CEE_Missing = false.
Proof. exact cee_golden_vector_alone_insufficient. Qed.

Lemma audit_cee_coq_alone_insufficient :
  candidate_equivalence_evidence_ready_from
    CEE_Missing CEE_Missing CEE_Sufficient CEE_Missing CEE_Missing = false.
Proof. exact cee_coq_alone_insufficient. Qed.

Lemma audit_cee_all_sufficient_allows_evidence_ready :
  candidate_equivalence_evidence_ready_from
    CEE_Sufficient CEE_Sufficient CEE_Sufficient
    CEE_Sufficient CEE_Sufficient = true.
Proof. exact cee_all_sufficient_allows_evidence_ready. Qed.

(** ---- CPP-01 gate alignment (both frozen false) ---- *)

Lemma audit_cpp01_candidate_gate_baseline_aligned :
  audit_cpp01_candidate_gate = false /\
  audit_candidate_equivalence_evidence_ready = false.
Proof.
  split; [exact cpp01_candidate_equivalence_ready_baseline |
          exact candidate_equivalence_evidence_ready_baseline_false].
Qed.

Lemma audit_cpp01_closable_implies_candidate_gate :
  forall catalog,
  pd4f_cpp01_closable catalog = true ->
  cpp01_candidate_equivalence_ready = true.
Proof.
  intros catalog H.
  unfold pd4f_cpp01_closable in H.
  destruct (pd4f_cpp01_ob02_ready_required catalog) eqn:E; [|discriminate H].
  destruct cpp01_runtime_alignment_ready eqn:Er; [|discriminate H].
  destruct cpp01_vector_alignment_ready eqn:Ev; [|discriminate H].
  destruct cpp01_coq_alignment_ready eqn:Ec; [|discriminate H].
  destruct cpp01_candidate_equivalence_ready eqn:Ek; [|discriminate H].
  reflexivity.
Qed.

(** ---- Readiness dependency re-exports ---- *)

Lemma audit_candidate_equivalence_ready_requires_ob02 :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_ready (dependency_status PD4F_OB_02 catalog) = true.
Proof. exact candidate_equivalence_ready_requires_ob02. Qed.

Lemma audit_candidate_equivalence_ready_forbids_cpp01_closed :
  forall catalog,
  candidate_equivalence_ready catalog = true ->
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false.
Proof. exact candidate_equivalence_ready_forbids_cpp01_closed. Qed.

Lemma audit_candidate_equivalence_not_equivalence :
  candidate_equivalence_not_equivalence.
Proof. exact candidate_equivalence_not_equivalence_holds. Qed.

Lemma audit_candidate_equivalence_requires_cpp01 :
  candidate_equivalence_requires_cpp01.
Proof. exact candidate_equivalence_requires_cpp01_holds. Qed.

(** ---- Master audit verdict ---- *)

Definition candidate_equivalence_evidence_audit_verdict : Prop :=
  candidate_equivalence_evidence_model_established /\
  audit_cee_draw_prohibition_active = true /\
  cee_state_missing (candidate_equivalence_evidence_state CEE_Runtime) = true /\
  cee_state_missing (candidate_equivalence_evidence_state CEE_GoldenVector) = true /\
  cee_state_missing (candidate_equivalence_evidence_state CEE_Coq) = true /\
  audit_candidate_equivalence_evidence_ready = false /\
  audit_candidate_equivalence_ready = false /\
  audit_cpp01_candidate_gate = false /\
  equivalence_claimed = false /\
  (forall catalog,
     candidate_equivalence_ready catalog = true ->
     ds_ready (dependency_status PD4F_OB_02 catalog) = true) /\
  (forall catalog,
     candidate_equivalence_ready catalog = true ->
     ds_closed (dependency_status PD4F_CPP_01 catalog) = false).

Lemma candidate_equivalence_evidence_audit_verdict_holds :
  candidate_equivalence_evidence_audit_verdict.
Proof.
  split. { exact candidate_equivalence_evidence_model_established_holds. }
  split. { reflexivity. }
  split. { exact cee_baseline_runtime_missing. }
  split. { exact cee_baseline_golden_vector_missing. }
  split. { exact cee_baseline_coq_missing. }
  split. { exact candidate_equivalence_evidence_ready_baseline_false. }
  split. { destruct candidate_equivalence_baseline_false as [H _]. exact H. }
  split. { exact cpp01_candidate_equivalence_ready_baseline. }
  split. { exact equivalence_not_claimed. }
  split. { exact candidate_equivalence_ready_requires_ob02. }
  exact candidate_equivalence_ready_forbids_cpp01_closed.
Qed.
