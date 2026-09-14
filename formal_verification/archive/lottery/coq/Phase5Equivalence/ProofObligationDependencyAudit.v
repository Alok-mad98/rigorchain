(** Phase5Equivalence/ProofObligationDependencyAudit.v
    Audit layer for PD-4F-OB-02 dependency model (TASK-PROOF-019 / PROOF-023).

    Re-exports the A-1/A-2/A-3 → PD-13/14/15 → PD4F_OB_02 → PD4F_CPP_01 graph and
    proves the frozen baseline matches PROOF-009 / PROOF-010 honesty boundaries.
    No obligation discharge. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.

(** ---- Audit exports ---- *)

Definition poda_object_count : nat :=
  List.length pd4f_ob02_dependency_catalog.

Definition poda_edge_count : nat :=
  List.length dependency_edge_catalog.

Definition audit_proof_obligation_02_closable : bool :=
  proof_obligation_02_closable_frozen.

Definition audit_pd4f_ob02_status : DependencyStatus :=
  pd4f_ob02_status.

Definition audit_pd4f_cpp01_status : DependencyStatus :=
  pd4f_cpp01_status.

(** ---- Partition / shape lemmas ---- *)

Lemma poda_object_count_value : poda_object_count = 10%nat.
Proof. exact pd4f_ob02_dependency_catalog_count. Qed.

Lemma poda_edge_count_value : poda_edge_count = 7%nat.
Proof. exact dependency_edge_catalog_count. Qed.

Lemma audit_baseline_a1_a2_a3_partial :
  ds_partial (status_of A1_SigmaByteOrder) = true /\
  ds_partial (status_of A2_Uint512Bound) = true /\
  ds_partial (status_of A3_FMembershipPredicate) = true.
Proof. exact baseline_a1_a2_a3_partial. Qed.

Lemma audit_baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open :
  ds_closed pd13_status = true /\
  ds_closed pd14_status = true /\
  ds_closed pd15_status = true /\
  ds_ready pd4f_ob02_status = true /\
  ds_open pd4f_cpp01_status = true.
Proof. exact baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open. Qed.

Lemma audit_baseline_pd4f_ob02_ready :
  ds_ready audit_pd4f_ob02_status = true.
Proof. exact baseline_pd4f_ob02_ready. Qed.

Lemma audit_baseline_pd4f_cpp01_open :
  ds_open audit_pd4f_cpp01_status = true.
Proof. exact baseline_pd4f_cpp01_open. Qed.

Lemma audit_proof_obligation_02_closable_at_baseline :
  audit_proof_obligation_02_closable = true.
Proof. exact proof_obligation_02_closable_at_baseline. Qed.

Lemma audit_pd4f_ob02_ready_at_baseline :
  pd4f_ob02_ready_frozen = true.
Proof. exact pd4f_ob02_ready_at_baseline. Qed.

Lemma audit_pd4f_ob02_dependency_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_may_enter_ready true = true /\
  pd4f_ob02_not_permitted_closed true = true /\
  pd4f_ob02_permitted_status true <> DS_Closed.
Proof. exact pd4f_ob02_dependency_soundness. Qed.

Lemma audit_pd4f_ob02_not_closed_without_cpp01 :
  forall catalog,
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
  pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed.
Proof.
  intros catalog Hcpp.
  destruct (pd4f_ob02_not_closed_without_cpp01 catalog Hcpp) as [Hnot [Hne Hgate]].
  exact Hne.
Qed.

Lemma audit_pd4f_ob02_does_not_establish_equivalence :
  equivalence_claimed = false /\
  audit_proof_obligation_02_closable = true /\
  ds_ready audit_pd4f_ob02_status = true /\
  ds_open audit_pd4f_cpp01_status = true.
Proof. exact pd4f_ob02_does_not_establish_equivalence. Qed.

(** ---- Master audit verdict ---- *)

Definition proof_obligation_dependency_audit_verdict : Prop :=
  proof_obligation_dependency_established /\
  poda_object_count = 10%nat /\
  poda_edge_count = 7%nat /\
  ds_ready audit_pd4f_ob02_status = true /\
  ds_open audit_pd4f_cpp01_status = true /\
  audit_proof_obligation_02_closable = true /\
  pd4f_ob02_ready_frozen = true /\
  equivalence_claimed = false /\
  (DS_Ready <> DS_Closed) /\
  (forall catalog,
     proof_obligation_02_closable catalog = true ->
     pd4f_ob02_permitted_status true = DS_Ready /\
     pd4f_ob02_permitted_status true <> DS_Closed).

Lemma proof_obligation_dependency_audit_verdict_holds :
  proof_obligation_dependency_audit_verdict.
Proof.
  split.
  { exact I. }
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [vm_compute; reflexivity|].
  split; [exact equivalence_not_claimed |].
  split; [exact ds_ready_ne_ds_closed |].
  intros cat Hclosable.
  destruct (pd4f_ob02_dependency_soundness cat Hclosable) as [_ [_ Hne]].
  split; [reflexivity | exact Hne].
Qed.
