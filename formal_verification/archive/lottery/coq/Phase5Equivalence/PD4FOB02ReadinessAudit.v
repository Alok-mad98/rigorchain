(** Phase5Equivalence/PD4FOB02ReadinessAudit.v
    Audit layer for PD-4F-OB-02 readiness closure (TASK-PROOF-023).

    Re-exports readiness soundness and verifies the frozen baseline:
    PD-13/14/15 closed, PD4F_OB_02 ready, PD4F_CPP_01 open.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.
Require Import Rigor.Phase5Equivalence.PD4FOB02Readiness.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition audit_pd4f_ob02_ready : bool := pd4f_ob02_ready_frozen.

Definition audit_pd13_baseline_closed : bool := pd13_baseline_closed.

Definition audit_pd14_baseline_closed : bool := pd14_baseline_closed.

Definition audit_pd15_baseline_closed : bool := pd15_baseline_closed.

Definition audit_pd4f_ob02_baseline_ready : bool := pd4f_ob02_baseline_ready.

Definition audit_pd4f_cpp01_baseline_open : bool := pd4f_cpp01_baseline_open.

(** ---- Readiness lemma re-exports ---- *)

Lemma audit_pd4f_ob02_ready_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_ready catalog = true /\
  pd4f_ob02_permitted_status true = DS_Ready /\
  pd4f_ob02_permitted_status true <> DS_Closed.
Proof.
  intros catalog Hclosable.
  destruct (pd4f_ob02_ready_soundness catalog Hclosable) as [Hready [ _ [ _ [Heq Hne]]]].
  repeat split; assumption.
Qed.

Lemma audit_pd4f_ob02_ready_not_closed :
  (ds_ready pd4f_ob02_status = true) /\
  (ds_closed pd4f_ob02_status = false) /\
  (DS_Ready <> DS_Closed).
Proof. exact pd4f_ob02_ready_not_closed. Qed.

Lemma audit_pd4f_ob02_not_closed_without_cpp01 :
  forall catalog,
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
  pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed.
Proof.
  intros catalog Hcpp.
  destruct (pd4f_ob02_not_closed_without_cpp01_holds catalog Hcpp) as [Hnot [Hne Hgate]].
  exact Hne.
Qed.

Lemma audit_pd4f_cpp01_unresolved :
  audit_pd4f_cpp01_baseline_open = true /\
  ds_closed pd4f_cpp01_status = false.
Proof.
  split; [exact pd4f_cpp01_baseline_open_true | vm_compute; reflexivity].
Qed.

Lemma audit_pd4f_ob02_readiness_from_closable :
  pd4f_ob02_readiness_from_closable.
Proof. exact pd4f_ob02_readiness_from_closable_holds. Qed.

(** ---- Dependency baseline (PD-13/14/15 closed; OB-02 ready; CPP-01 open) ---- *)

Lemma audit_pd13_dependency_status_closed :
  audit_pd13_baseline_closed = true.
Proof. exact pd13_baseline_closed_true. Qed.

Lemma audit_pd14_dependency_status_closed :
  audit_pd14_baseline_closed = true.
Proof. exact pd14_baseline_closed_true. Qed.

Lemma audit_pd15_dependency_status_closed :
  audit_pd15_baseline_closed = true.
Proof. exact pd15_baseline_closed_true. Qed.

Lemma audit_pd4f_ob02_dependency_status_ready :
  audit_pd4f_ob02_baseline_ready = true.
Proof. exact pd4f_ob02_baseline_ready_true. Qed.

Lemma audit_pd4f_cpp01_dependency_status_open :
  audit_pd4f_cpp01_baseline_open = true.
Proof. exact pd4f_cpp01_baseline_open_true. Qed.

Lemma audit_pd4f_ob02_does_not_establish_equivalence :
  pd4f_ob02_equivalence_claimed_flag = false /\
  audit_pd4f_ob02_ready = true.
Proof.
  split; [exact pd4f_ob02_equivalence_claimed_false | exact pd4f_ob02_ready_at_baseline].
Qed.

(** ---- Master audit verdict ---- *)

Definition pd4f_ob02_readiness_audit_verdict : Prop :=
  pd4f_ob02_readiness_established /\
  audit_pd4f_ob02_ready = true /\
  audit_pd13_baseline_closed = true /\
  audit_pd14_baseline_closed = true /\
  audit_pd15_baseline_closed = true /\
  audit_pd4f_ob02_baseline_ready = true /\
  audit_pd4f_cpp01_baseline_open = true /\
  pd4f_ob02_equivalence_claimed_flag = false /\
  (DS_Ready <> DS_Closed) /\
  (forall catalog,
     ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
     pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed).

Lemma pd4f_ob02_readiness_audit_verdict_holds :
  pd4f_ob02_readiness_audit_verdict.
Proof.
  split; [exact pd4f_ob02_readiness_established_holds |].
  split; [exact pd4f_ob02_ready_at_baseline |].
  split; [exact pd13_baseline_closed_true |].
  split; [exact pd14_baseline_closed_true |].
  split; [exact pd15_baseline_closed_true |].
  split; [exact pd4f_ob02_baseline_ready_true |].
  split; [exact pd4f_cpp01_baseline_open_true |].
  split; [exact pd4f_ob02_equivalence_claimed_false |].
  split; [exact ds_ready_ne_ds_closed |].
  intros catalog Hcpp.
  destruct (pd4f_ob02_not_closed_without_cpp01_holds catalog Hcpp) as [Hnot [Hne Hgate]].
  exact Hne.
Qed.
