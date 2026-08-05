(** Phase5Equivalence/SigmaIntegerRefinementAudit.v
    Audit layer for PD-15 sigma integer refinement (TASK-PROOF-020).

    Re-exports PROOF-011 A-1 little-endian bridge lemmas and verifies PD-15
    may enter [DS_Closed] while PD-13/PD-14 stay closed and PD4F_OB_02 stays open.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Require Import Rigor.RigorBytes.
Require Import Rigor.Phase5Equivalence.SigmaIntegerRefinement.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition sia_sigma_byte_length : nat := sigma_byte_length.

Definition audit_pd15_closable : bool := pd15_closable.

Definition audit_pd15_baseline_closed : bool := pd15_baseline_closed.

Definition audit_pd14_baseline_closed : bool := pd14_baseline_closed.

Definition audit_pd13_baseline_closed : bool := pd13_baseline_closed.

Definition audit_pd4f_ob02_baseline_ready : bool := pd4f_ob02_baseline_ready.

Definition audit_pd4f_cpp01_baseline_open : bool := pd4f_cpp01_baseline_open.

(** ---- Refinement lemma re-exports ---- *)

Lemma audit_sigma_bytes_to_integer_deterministic :
  forall (bs1 bs2 : list byte),
  bs1 = bs2 ->
  sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2.
Proof. intros bs1 bs2 Heq. subst bs2. reflexivity. Qed.

Lemma audit_sigma_bytes_to_integer_injective_length_fixed :
  forall (bs1 bs2 : list byte),
  List.length bs1 = sigma_byte_length ->
  List.length bs2 = sigma_byte_length ->
  is_sigma_bytes (bs1 : list byte) = true ->
  is_sigma_bytes (bs2 : list byte) = true ->
  sigma_bytes_to_integer bs1 = sigma_bytes_to_integer bs2 ->
  bs1 = bs2.
Proof.
  intros bs1 bs2 H1 H2 Hok1 Hok2 Heq.
  apply (sigma_bytes_to_integer_injective_length_fixed bs1 bs2 H1 H2 Hok1 Hok2 Heq).
Qed.

Lemma audit_runtime_spec_consistency :
  forall (bs : list byte),
  is_sigma_bytes bs = true ->
  sigma_bytes_to_integer bs = uint256_of_bytes bs.
Proof.
  intros bs Hob.
  apply (runtime_spec_consistency bs Hob).
Qed.

Lemma audit_pd15_refinement_lemmas_discharged :
  pd15_refinement_lemmas_discharged.
Proof. exact pd15_refinement_lemmas_discharged_holds. Qed.

Lemma audit_pd15_refinement_soundness :
  pd15_closure_from_closable.
Proof. exact pd15_closure_from_closable_holds. Qed.

(** ---- Dependency baseline (PD-13/14/15 closed; PD4F open) ---- *)

Lemma audit_pd15_dependency_status_closed :
  audit_pd15_baseline_closed = true.
Proof. exact pd15_baseline_closed_true. Qed.

Lemma audit_pd14_dependency_status_closed :
  audit_pd14_baseline_closed = true.
Proof. exact pd14_baseline_closed_true. Qed.

Lemma audit_pd13_dependency_status_closed :
  audit_pd13_baseline_closed = true.
Proof. exact pd13_baseline_closed_true. Qed.

Lemma audit_pd4f_ob02_dependency_status_ready :
  audit_pd4f_ob02_baseline_ready = true.
Proof. exact pd4f_ob02_baseline_ready_true. Qed.

Lemma audit_pd4f_cpp01_dependency_status_open :
  audit_pd4f_cpp01_baseline_open = true.
Proof. exact pd4f_cpp01_baseline_open_true. Qed.

Lemma audit_pd15_does_not_establish_equivalence :
  pd15_equivalence_claimed_flag = false /\
  pd15_ob02_closable_flag = true.
Proof.
  split; [exact pd15_equivalence_claimed_false | exact pd15_ob02_closable_flag_true].
Qed.

(** ---- Master audit verdict ---- *)

Definition sigma_integer_refinement_audit_verdict : Prop :=
  sigma_integer_refinement_established /\
  sia_sigma_byte_length = 32%nat /\
  audit_pd15_closable = true /\
  audit_pd15_baseline_closed = true /\
  audit_pd14_baseline_closed = true /\
  audit_pd13_baseline_closed = true /\
  audit_pd4f_ob02_baseline_ready = true /\
  audit_pd4f_cpp01_baseline_open = true /\
  pd15_equivalence_claimed_flag = false /\
  pd15_ob02_closable_flag = true /\
  pd15_refinement_lemmas_discharged.

Lemma sigma_integer_refinement_audit_verdict_holds :
  sigma_integer_refinement_audit_verdict.
Proof.
  split; [exact sigma_integer_refinement_established_holds |].
  split; [vm_compute; reflexivity |].
  split; [vm_compute; reflexivity |].
  split; [exact pd15_baseline_closed_true |].
  split; [exact pd14_baseline_closed_true |].
  split; [exact pd13_baseline_closed_true |].
  split; [exact pd4f_ob02_baseline_ready_true |].
  split; [exact pd4f_cpp01_baseline_open_true |].
  split; [exact pd15_equivalence_claimed_false |].
  split; [exact pd15_ob02_closable_flag_true |].
  exact pd15_refinement_lemmas_discharged_holds.
Qed.
