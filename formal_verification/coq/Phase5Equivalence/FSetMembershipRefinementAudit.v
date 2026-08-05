(** Phase5Equivalence/FSetMembershipRefinementAudit.v
    Audit layer for PD-13 F-set membership refinement (TASK-PROOF-022).

    Re-exports PROOF-011 A-3 F membership predicate lemmas and verifies PD-13
    may enter [DS_Closed] while PD-14/PD-15 stay closed and PD4F_OB_02 stays open.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String ZArith.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.FSetMembershipRefinement.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition fma_min_convergence_amount : Z := min_convergence_amount.

Definition audit_pd13_closable : bool := pd13_closable.

Definition audit_pd13_baseline_closed : bool := pd13_baseline_closed.

Definition audit_pd14_baseline_closed : bool := pd14_baseline_closed.

Definition audit_pd15_baseline_closed : bool := pd15_baseline_closed.

Definition audit_pd4f_ob02_baseline_ready : bool := pd4f_ob02_baseline_ready.

Definition audit_pd4f_cpp01_baseline_open : bool := pd4f_cpp01_baseline_open.

(** ---- Refinement lemma re-exports ---- *)

Lemma audit_coq_enumerate_F_deterministic :
  forall txs1 txs2,
  txs1 = txs2 ->
  coq_enumerate_F txs1 = coq_enumerate_F txs2.
Proof.
  intros txs1 txs2 Heq.
  apply (coq_enumerate_F_deterministic txs1 txs2 Heq).
Qed.

Lemma audit_F_membership_deterministic :
  forall txid F1 F2,
  F1 = F2 ->
  F_membership txid F1 = F_membership txid F2.
Proof.
  intros txid F1 F2 Heq.
  apply (F_membership_deterministic txid F1 F2 Heq).
Qed.

Lemma audit_coq_enumerate_F_nodup :
  forall txs,
  NoDup (map fmt_txid_bytes txs) ->
  NoDup (coq_enumerate_F txs).
Proof.
  intros txs Hmap.
  apply (coq_enumerate_F_nodup txs Hmap).
Qed.

Lemma audit_runtime_spec_consistency :
  forall txs,
  coq_enumerate_F txs = fvr_enumerate_F txs.
Proof.
  intros txs.
  apply (runtime_spec_consistency txs).
Qed.

Lemma audit_F_membership_sound :
  forall tx txs,
  tx_passes_f_predicate tx = true ->
  In tx txs ->
  F_membership (fmt_txid_bytes tx) (coq_enumerate_F txs) = true.
Proof.
  intros tx txs Hpass Hin.
  apply (F_membership_sound tx txs Hpass Hin).
Qed.

Lemma audit_pd13_refinement_lemmas_discharged :
  pd13_refinement_lemmas_discharged.
Proof. exact pd13_refinement_lemmas_discharged_holds. Qed.

Lemma audit_pd13_refinement_soundness :
  pd13_closure_from_closable.
Proof. exact pd13_closure_from_closable_holds. Qed.

(** ---- Dependency baseline (PD-13/14/15 closed; PD4F open) ---- *)

Lemma audit_pd13_dependency_status_closed :
  audit_pd13_baseline_closed = true.
Proof. exact pd13_baseline_closed_true. Qed.

Lemma audit_pd14_dependency_status_closed :
  audit_pd14_baseline_closed = true.
Proof. exact pd14_baseline_closed_from_catalog. Qed.

Lemma audit_pd15_dependency_status_closed :
  audit_pd15_baseline_closed = true.
Proof. exact pd15_baseline_closed_from_catalog. Qed.

Lemma audit_pd4f_ob02_dependency_status_ready :
  audit_pd4f_ob02_baseline_ready = true.
Proof. exact pd4f_ob02_baseline_ready_true. Qed.

Lemma audit_pd4f_cpp01_dependency_status_open :
  audit_pd4f_cpp01_baseline_open = true.
Proof. exact pd4f_cpp01_baseline_open_true. Qed.

Lemma audit_pd13_does_not_establish_equivalence :
  pd13_equivalence_claimed_flag = false /\
  pd13_ob02_closable_flag = true.
Proof.
  split; [exact pd13_equivalence_claimed_false | exact pd13_ob02_closable_flag_true].
Qed.

(** ---- Master audit verdict ---- *)

Definition fset_membership_refinement_audit_verdict : Prop :=
  fset_membership_refinement_established /\
  fma_min_convergence_amount = 100000000%Z /\
  audit_pd13_closable = true /\
  audit_pd13_baseline_closed = true /\
  audit_pd14_baseline_closed = true /\
  audit_pd15_baseline_closed = true /\
  audit_pd4f_ob02_baseline_ready = true /\
  audit_pd4f_cpp01_baseline_open = true /\
  pd13_equivalence_claimed_flag = false /\
  pd13_ob02_closable_flag = true /\
  pd13_refinement_lemmas_discharged.

Lemma fset_membership_refinement_audit_verdict_holds :
  fset_membership_refinement_audit_verdict.
Proof.
  split; [exact fset_membership_refinement_established_holds |].
  split; [vm_compute; reflexivity |].
  split; [vm_compute; reflexivity |].
  split; [exact pd13_baseline_closed_true |].
  split; [exact pd14_baseline_closed_from_catalog |].
  split; [exact pd15_baseline_closed_from_catalog |].
  split; [exact pd4f_ob02_baseline_ready_true |].
  split; [exact pd4f_cpp01_baseline_open_true |].
  split; [exact pd13_equivalence_claimed_false |].
  split; [exact pd13_ob02_closable_flag_true |].
  exact pd13_refinement_lemmas_discharged_holds.
Qed.
