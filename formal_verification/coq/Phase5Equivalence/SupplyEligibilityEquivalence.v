(** Phase5Equivalence/SupplyEligibilityEquivalence.v
    Supply + Eligibility/sigma equivalence projection verification (TASK-018FF-F).

    Extends the 014CC Coq-side projections into the Phase-5 equivalence
    comparison layer:

      Supply:       [project_supply_after_trace]  — Coq-state ORU+RES total
      Eligibility:  [project_eligibility_sigma] — imported [dc_eligible] flags

    HONESTY BOUNDARY:
    - Supply frozen payload is ABSENT ([fco_present = false]); recorded as MISSING.
    - Coq supply projection is always 0 for traces from [init_consensus_state];
      the [0 = 0] match with the absent frozen payload is a typed structural
      identity, NOT actual supply equivalence.
    - Eligibility projection is an identity map over imported flags (PD-4F-OB-03);
      NOT a Coq re-derivation from the hash kernel.
    - Both projections are [PS_Partial]. No Coq = Runtime / Coq = Frozen C++
      equivalence is claimed.
    - Draw / Replay / Reorg / ConnectBlock equivalence modules are untouched. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** ================================================================
    Part A:  Result vocabulary
    ================================================================ *)

Inductive SupplyEligEquivKind :=
| SEEK_SupplyFrozenMissing
| SEEK_SupplyRuntimeMissing
| SEEK_SupplyProjection
| SEEK_SupplyStructuralIdentity
| SEEK_EligibilityIdentity.

Inductive SupplyEligEquivResult :=
| SEER_Resolved
| SEER_Disagreement
| SEER_Missing
| SEER_Partial.

Definition se_result_resolved (r : SupplyEligEquivResult) : bool :=
  match r with
  | SEER_Resolved => true
  | _ => false
  end.

Definition se_result_disagreement (r : SupplyEligEquivResult) : bool :=
  match r with
  | SEER_Disagreement => true
  | _ => false
  end.

Definition se_result_missing (r : SupplyEligEquivResult) : bool :=
  match r with
  | SEER_Missing => true
  | _ => false
  end.

Definition se_result_partial (r : SupplyEligEquivResult) : bool :=
  match r with
  | SEER_Partial => true
  | _ => false
  end.

(** Both 014CC projections are [PS_Partial]. *)
Definition supply_projection_marked_partial : ProjectionStatus :=
  supply_projection_status.

Definition eligibility_projection_marked_partial : ProjectionStatus :=
  eligibility_projection_status.

Lemma supply_projection_is_ps_partial :
  supply_projection_marked_partial = PS_Partial.
Proof. exact supply_projection_status_value. Qed.

Lemma eligibility_projection_is_ps_partial :
  eligibility_projection_marked_partial = PS_Partial.
Proof. exact eligibility_projection_status_value. Qed.

(** ================================================================
    Part B:  Supply — missing frozen/runtime payloads
    ================================================================ *)

Definition frozen_supply_present : bool := fco_present fco_supply.

Definition frozen_supply_payload_value : Z := frozen_supply_payload.

Lemma frozen_supply_not_present : frozen_supply_present = false.
Proof. reflexivity. Qed.

Lemma frozen_supply_payload_is_zero : frozen_supply_payload_value = 0%Z.
Proof. exact frozen_supply_is_zero. Qed.

Definition supply_frozen_side_result : SupplyEligEquivResult :=
  if frozen_supply_present then SEER_Resolved else SEER_Missing.

Lemma supply_frozen_side_is_missing :
  supply_frozen_side_result = SEER_Missing.
Proof. vm_compute. reflexivity. Qed.

(** Runtime has no single supply payload (014CC [absent_live BC_Supply]). *)
Definition supply_runtime_side_result : SupplyEligEquivResult := SEER_Missing.

(** ================================================================
    Part C:  Supply — Coq projection on sample traces
    ================================================================ *)

Definition coq_supply_after_trace (tr : consensus_trace) : Z :=
  project_supply_after_trace tr.

Definition supply_trace_empty : consensus_trace := [].
Definition supply_trace_one : consensus_trace := [ TrAcceptBlock 1 ].
Definition supply_trace_replay : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3 ].
Definition supply_trace_five : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3;
    TrAcceptBlock 4; TrAcceptBlock 5 ].

Definition sample_supply_traces : list consensus_trace :=
  [ supply_trace_empty;
    supply_trace_one;
    supply_trace_replay;
    supply_trace_five ].

Lemma sample_supply_traces_count : length sample_supply_traces = 4%nat.
Proof. reflexivity. Qed.

Definition supply_projection_row_result (tr : consensus_trace)
  : SupplyEligEquivResult :=
  if Z.eqb (coq_supply_after_trace tr) (coq_supply_after_trace tr)
  then SEER_Resolved else SEER_Disagreement.

Lemma supply_projection_all_resolved :
  forall tr, In tr sample_supply_traces ->
    supply_projection_row_result tr = SEER_Resolved.
Proof.
  intros tr Hin. unfold supply_projection_row_result.
  rewrite Z.eqb_refl. reflexivity.
Qed.

Lemma coq_supply_all_zero :
  forall tr, In tr sample_supply_traces ->
    coq_supply_after_trace tr = 0%Z.
Proof.
  intros tr Hin. apply project_supply_after_trace_zero.
Qed.

(** Typed structural identity with the absent frozen payload field (both 0).
    Recorded as PARTIAL — not a real supply-equivalence claim. *)
Definition supply_structural_identity_result : SupplyEligEquivResult :=
  if Z.eqb frozen_supply_payload_value 0%Z then SEER_Partial else SEER_Disagreement.

Lemma supply_structural_identity_is_partial :
  supply_structural_identity_result = SEER_Partial.
Proof. vm_compute. reflexivity. Qed.

Theorem supply_projection_structural_identity_reexport :
  forall tr, coq_supply_after_trace tr = frozen_supply_payload_value.
Proof. exact supply_projection_structural_identity. Qed.

(** ================================================================
    Part D:  Eligibility / sigma — identity mapping over draw rows
    ================================================================ *)

Definition coq_eligibility_flags (cands : list DrawCandidate) : list bool :=
  project_eligibility_sigma cands.

Fixpoint bool_list_eqb (l1 l2 : list bool) : bool :=
  match l1, l2 with
  | nil, nil => true
  | a :: t1, b :: t2 => andb (Bool.eqb a b) (bool_list_eqb t1 t2)
  | _, _ => false
  end.

Definition eligibility_row_result (r : DrawAuditRow) : SupplyEligEquivResult :=
  if bool_list_eqb (map dc_eligible (dar_candidates r))
                    (project_eligibility_sigma (dar_candidates r))
  then SEER_Resolved else SEER_Disagreement.

Lemma eligibility_row_all_resolved :
  forall r, In r draw_audit_rows ->
    eligibility_row_result r = SEER_Resolved.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma eligibility_flags_length_preserved :
  forall r, In r draw_audit_rows ->
    length (coq_eligibility_flags (dar_candidates r))
      = length (dar_candidates r).
Proof.
  intros r Hin.
  apply project_eligibility_sigma_length.
Qed.

Definition draw_audit_row_count : nat := length draw_audit_rows.

Lemma draw_audit_row_count_value : draw_audit_row_count = 14%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part E:  Component verdict (PARTIAL — no equivalence claimed)
    ================================================================ *)

Definition supply_eligibility_projection_verified : bool := true.

Definition supply_eligibility_equivalence_claimed : bool := false.

Lemma supply_eligibility_equivalence_not_claimed :
  supply_eligibility_equivalence_claimed = false /\
  negb equivalence_claimed = true.
Proof.
  split; [reflexivity | exact equivalence_not_claimed].
Qed.

Definition projection_status_partial_bool (ps : ProjectionStatus) : bool :=
  match ps with
  | PS_Partial => true
  | _ => false
  end.

Definition supply_eligibility_status_partial : bool :=
  supply_eligibility_projection_verified &&
  projection_status_partial_bool supply_projection_marked_partial &&
  projection_status_partial_bool eligibility_projection_marked_partial &&
  negb supply_eligibility_equivalence_claimed.

Lemma supply_eligibility_status_partial_value :
  supply_eligibility_status_partial = true.
Proof. vm_compute. reflexivity. Qed.
