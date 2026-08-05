(** Phase5Equivalence/UInt512OverflowRefinementAudit.v
    Audit layer for PD-14 uint512 overflow refinement (TASK-PROOF-021).

    Re-exports PROOF-011 A-2 magnitude bridge lemmas and verifies PD-14
    may enter [DS_Closed] while PD-13/PD-15 stay closed and PD4F_OB_02 stays open.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Require Import Rigor.RigorBytes.
Require Import Rigor.Phase5Equivalence.UInt512OverflowRefinement.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition uoa_uint512_byte_length : nat := uint512_byte_length.

Definition audit_pd14_closable : bool := pd14_closable.

Definition audit_pd14_baseline_closed : bool := pd14_baseline_closed.

Definition audit_pd15_baseline_closed : bool := pd15_baseline_closed.

Definition audit_pd13_baseline_closed : bool := pd13_baseline_closed.

Definition audit_pd4f_ob02_baseline_ready : bool := pd4f_ob02_baseline_ready.

Definition audit_pd4f_cpp01_baseline_open : bool := pd4f_cpp01_baseline_open.

(** ---- Refinement lemma re-exports ---- *)

Lemma audit_mul256x256_full_deterministic :
  forall a1 a2 b1 b2,
  a1 = a2 ->
  b1 = b2 ->
  mul256x256_full a1 b1 = mul256x256_full a2 b2.
Proof.
  intros a1 a2 b1 b2 Ha Hb.
  apply (mul256x256_full_deterministic a1 a2 b1 b2 Ha Hb).
Qed.

Lemma audit_uint512_bytes_to_integer_injective_length_fixed :
  forall (bs1 bs2 : list byte),
  List.length bs1 = uint512_byte_length ->
  List.length bs2 = uint512_byte_length ->
  is_uint512_bytes (bs1 : list byte) = true ->
  is_uint512_bytes (bs2 : list byte) = true ->
  uint512_bytes_to_integer bs1 = uint512_bytes_to_integer bs2 ->
  bs1 = bs2.
Proof.
  intros bs1 bs2 H1 H2 Hok1 Hok2 Heq.
  apply (uint512_bytes_to_integer_injective_length_fixed bs1 bs2 H1 H2 Hok1 Hok2 Heq).
Qed.

Lemma audit_runtime_spec_consistency :
  forall a b,
  uint256_value_bound a = true ->
  uint256_value_bound b = true ->
  mul256x256_full a b = fvr_mul256x256_full a b /\
  uint512_value_bound (mul256x256_full a b) = true.
Proof.
  intros a b Ha Hb.
  apply (runtime_spec_consistency a b Ha Hb).
Qed.

Lemma audit_pd14_refinement_lemmas_discharged :
  pd14_refinement_lemmas_discharged.
Proof. exact pd14_refinement_lemmas_discharged_holds. Qed.

Lemma audit_pd14_refinement_soundness :
  pd14_closure_from_closable.
Proof. exact pd14_closure_from_closable_holds. Qed.

(** ---- Dependency baseline (PD-13/14/15 closed; PD4F open) ---- *)

Lemma audit_pd14_dependency_status_closed :
  audit_pd14_baseline_closed = true.
Proof. exact pd14_baseline_closed_true. Qed.

Lemma audit_pd15_dependency_status_closed :
  audit_pd15_baseline_closed = true.
Proof. exact pd15_baseline_closed_from_catalog. Qed.

Lemma audit_pd13_dependency_status_closed :
  audit_pd13_baseline_closed = true.
Proof. exact pd13_baseline_closed_true. Qed.

Lemma audit_pd4f_ob02_dependency_status_ready :
  audit_pd4f_ob02_baseline_ready = true.
Proof. exact pd4f_ob02_baseline_ready_true. Qed.

Lemma audit_pd4f_cpp01_dependency_status_open :
  audit_pd4f_cpp01_baseline_open = true.
Proof. exact pd4f_cpp01_baseline_open_true. Qed.

Lemma audit_pd14_does_not_establish_equivalence :
  pd14_equivalence_claimed_flag = false /\
  pd14_ob02_closable_flag = true.
Proof.
  split; [exact pd14_equivalence_claimed_false | exact pd14_ob02_closable_flag_true].
Qed.

(** ---- Master audit verdict ---- *)

Definition uint512_overflow_refinement_audit_verdict : Prop :=
  uint512_overflow_refinement_established /\
  uoa_uint512_byte_length = 64%nat /\
  audit_pd14_closable = true /\
  audit_pd14_baseline_closed = true /\
  audit_pd15_baseline_closed = true /\
  audit_pd13_baseline_closed = true /\
  audit_pd4f_ob02_baseline_ready = true /\
  audit_pd4f_cpp01_baseline_open = true /\
  pd14_equivalence_claimed_flag = false /\
  pd14_ob02_closable_flag = true /\
  pd14_refinement_lemmas_discharged.

Lemma uint512_overflow_refinement_audit_verdict_holds :
  uint512_overflow_refinement_audit_verdict.
Proof.
  split; [exact uint512_overflow_refinement_established_holds |].
  split; [vm_compute; reflexivity |].
  split; [vm_compute; reflexivity |].
  split; [exact pd14_baseline_closed_true |].
  split; [exact pd15_baseline_closed_from_catalog |].
  split; [exact pd13_baseline_closed_true |].
  split; [exact pd4f_ob02_baseline_ready_true |].
  split; [exact pd4f_cpp01_baseline_open_true |].
  split; [exact pd14_equivalence_claimed_false |].
  split; [exact pd14_ob02_closable_flag_true |].
  exact pd14_refinement_lemmas_discharged_holds.
Qed.
