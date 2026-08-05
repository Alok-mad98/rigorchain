(** Phase5Equivalence/PD4FOB02Readiness.v
    PD-4F-OB-02 readiness closure (TASK-PROOF-023).

    Proves that when PD-13/14/15 are [DS_Closed], PD4F_OB_02 may enter [DS_Ready]
    but not [DS_Closed] while PD4F_CPP_01 remains unresolved.

    HONESTY BOUNDARY:
    - Coq readiness model only; no consensus / runtime / FVR changes.
    - [equivalence_claimed] remains false.
    - PD4F_CPP_01 stays [DS_Open]. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.

(** ================================================================
    Part A:  Readiness predicate (re-exported from catalog)
    ================================================================ *)

(** [pd4f_ob02_ready catalog] holds when PD-13/14/15 are all [DS_Closed]
    and the dependency model permits [DS_Ready] for PD4F_OB_02. *)

(** ================================================================
    Part B:  Readiness soundness
    ================================================================ *)

Theorem pd4f_ob02_ready_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_ready catalog = true /\
  pd4f_ob02_may_enter_ready true = true /\
  ds_ready (pd4f_ob02_permitted_status true) = true /\
  pd4f_ob02_permitted_status true = DS_Ready /\
  pd4f_ob02_permitted_status true <> DS_Closed.
Proof.
  intros catalog Hclosable.
  destruct (pd4f_ob02_dependency_soundness catalog Hclosable) as [Hready [Hnot Hne]].
  split; [| split; [| split; [| split]]].
  - unfold pd4f_ob02_ready. rewrite Hclosable. simpl. exact Hready.
  - exact Hready.
  - simpl. reflexivity.
  - simpl. reflexivity.
  - exact Hne.
Qed.

Lemma pd4f_ob02_ready_soundness_frozen :
  pd4f_ob02_ready_frozen = true /\
  ds_ready pd4f_ob02_status = true /\
  pd4f_ob02_status = DS_Ready /\
  pd4f_ob02_status <> DS_Closed.
Proof.
  split; [| split; [| split]].
  - exact pd4f_ob02_ready_at_baseline.
  - exact baseline_pd4f_ob02_ready.
  - vm_compute. reflexivity.
  - intro H. vm_compute in H. discriminate H.
Qed.

(** ================================================================
    Part C:  [DS_Ready] ≠ [DS_Closed] and CPP-01 blocker
    ================================================================ *)

Lemma pd4f_ob02_ready_not_closed :
  (ds_ready pd4f_ob02_status = true) /\
  (ds_closed pd4f_ob02_status = false) /\
  (DS_Ready <> DS_Closed).
Proof.
  split; [| split].
  - exact baseline_pd4f_ob02_ready.
  - vm_compute. reflexivity.
  - exact ds_ready_ne_ds_closed.
Qed.

Lemma pd4f_cpp01_unresolved_at_baseline :
  ds_open pd4f_cpp01_status = true /\
  ds_closed pd4f_cpp01_status = false.
Proof.
  split; vm_compute; reflexivity.
Qed.

Theorem pd4f_ob02_not_closed_without_cpp01_holds :
  forall catalog,
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
  pd4f_ob02_not_permitted_closed (proof_obligation_02_closable catalog) = true /\
  pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed /\
  pd4f_ob02_closure_gate catalog = false.
Proof. exact pd4f_ob02_not_closed_without_cpp01. Qed.

(** ================================================================
    Part D:  Frozen baseline exports
    ================================================================ *)

Definition pd13_baseline_closed : bool := ds_closed pd13_status.
Definition pd14_baseline_closed : bool := ds_closed pd14_status.
Definition pd15_baseline_closed : bool := ds_closed pd15_status.
Definition pd4f_ob02_baseline_ready : bool := ds_ready pd4f_ob02_status.
Definition pd4f_cpp01_baseline_open : bool := ds_open pd4f_cpp01_status.

Lemma pd13_baseline_closed_true : pd13_baseline_closed = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd14_baseline_closed_true : pd14_baseline_closed = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd15_baseline_closed_true : pd15_baseline_closed = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd4f_ob02_baseline_ready_true : pd4f_ob02_baseline_ready = true.
Proof. exact baseline_pd4f_ob02_ready. Qed.

Lemma pd4f_cpp01_baseline_open_true : pd4f_cpp01_baseline_open = true.
Proof. exact baseline_pd4f_cpp01_open. Qed.

Definition pd4f_ob02_equivalence_claimed_flag : bool := equivalence_claimed.

Lemma pd4f_ob02_equivalence_claimed_false :
  pd4f_ob02_equivalence_claimed_flag = false.
Proof. vm_compute. reflexivity. Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [pd4f_ob02_readiness_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
Definition pd4f_ob02_readiness_established : Prop := True.

Lemma pd4f_ob02_readiness_established_holds :
  pd4f_ob02_readiness_established.
Proof. exact I. Qed.

Definition pd4f_ob02_readiness_from_closable : Prop :=
  proof_obligation_02_closable_frozen = true ->
  pd4f_ob02_ready_frozen = true.

Lemma pd4f_ob02_readiness_from_closable_holds :
  pd4f_ob02_readiness_from_closable.
Proof.
  intros _. exact pd4f_ob02_ready_at_baseline.
Qed.
