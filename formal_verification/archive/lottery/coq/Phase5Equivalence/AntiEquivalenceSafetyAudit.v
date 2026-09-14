(** Phase5Equivalence/AntiEquivalenceSafetyAudit.v
    Anti-equivalence safety audit (TASK-018GG-E).

    Aggregates the 18-cell anti-equivalence tally from
    AntiEquivalenceSafety.v. Records that no equivalence is claimed;
    PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List Bool ZArith.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.AntiEquivalenceSafety.

(** ---- Counts ---- *)

Definition ae_count_total : nat := length all_ae_results.

Definition ae_aligned_count : nat :=
  length (filter aer_aligned all_ae_results).
Definition ae_misaligned_count : nat :=
  length (filter aer_domain_gap all_ae_results).
Definition ae_missing_count : nat :=
  length (filter aer_missing all_ae_results).
Definition ae_non_equivalence_count : nat :=
  length (filter aer_non_equivalence all_ae_results).

Lemma ae_count_total_value : ae_count_total = 18%nat.
Proof. exact all_ae_results_length. Qed.

Lemma ae_aligned_count_value : ae_aligned_count = 6%nat.
Proof. exact all_ae_results_aligned_count. Qed.

Lemma ae_misaligned_count_value : ae_misaligned_count = 0%nat.
Proof. exact all_ae_results_domain_gap_count. Qed.

Lemma ae_missing_count_value : ae_missing_count = 0%nat.
Proof. exact all_ae_results_missing_count. Qed.

Lemma ae_non_equivalence_count_value : ae_non_equivalence_count = 12%nat.
Proof. exact all_ae_results_non_equivalence_count. Qed.

Lemma audit_partition_complete :
  (ae_aligned_count + ae_misaligned_count
   + ae_missing_count + ae_non_equivalence_count)%nat
  = ae_count_total.
Proof. vm_compute. reflexivity. Qed.

(** ---- Honesty: no equivalence claims ---- *)

Definition audit_no_equivalence_claims : Prop :=
  equivalence_claimed = false /\
  negb ae_coq_eq_runtime_established = true /\
  negb ae_coq_eq_frozen_established = true /\
  (forall a b : AntiEquivalenceObservation,
     is_cross_coq_external a.(ae_source) b.(ae_source) = true ->
     check_anti_equivalence a b <> AER_StructurallyAligned).

Lemma audit_no_equivalence_claims_holds : audit_no_equivalence_claims.
Proof.
  unfold audit_no_equivalence_claims.
  split; [exact anti_equivalence_no_equivalence_leakage_global | ].
  split; [exact ae_coq_eq_runtime_not_established | ].
  split; [exact ae_coq_eq_frozen_not_established | ].
  exact anti_equivalence_no_false_equivalence_leakage.
Qed.

(** ---- Re-exports of required obligations ---- *)

Lemma audit_anti_equivalence_deterministic :
  forall a1 a2 b1 b2 : AntiEquivalenceObservation,
    a1 = a2 -> b1 = b2 ->
    check_anti_equivalence a1 b1 = check_anti_equivalence a2 b2.
Proof. exact anti_equivalence_deterministic. Qed.

Lemma audit_anti_equivalence_total :
  forall a b : AntiEquivalenceObservation,
    exists r, check_anti_equivalence a b = r.
Proof. exact anti_equivalence_total. Qed.

Lemma audit_anti_equivalence_preserves_component :
  forall a b : AntiEquivalenceObservation,
    check_anti_equivalence a b <> AER_Missing ->
    a.(ae_component) = b.(ae_component).
Proof. exact anti_equivalence_preserves_component. Qed.

Lemma audit_anti_equivalence_detects_domain_gaps :
  forall a b : AntiEquivalenceObservation,
    a.(ae_component) = b.(ae_component) ->
    a.(ae_source) <> b.(ae_source) ->
    is_cross_coq_external a.(ae_source) b.(ae_source) = true ->
    check_anti_equivalence a b = AER_ExplicitNonEquivalence.
Proof. exact anti_equivalence_detects_domain_gaps. Qed.

Lemma audit_anti_equivalence_circularity_recorded :
  anti_equivalence_circularity.(aec_imported_eligibility) = true /\
  anti_equivalence_circularity.(aec_payload_domain_gap) = true /\
  anti_equivalence_circularity.(aec_supply_absent) = true.
Proof. exact anti_equivalence_circularity_recorded. Qed.

(** ---- Honesty / debt re-exports ---- *)

Lemma audit_ae_pd_4f_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact anti_equivalence_pd_ob_02_open. Qed.

Lemma audit_ae_pd_4f_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact anti_equivalence_pd_cpp_01_open. Qed.

Lemma audit_ae_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact anti_equivalence_no_equivalence_claimed. Qed.

Lemma audit_ae_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

(** ---- Master audit verdict ---- *)

Definition anti_equivalence_safety_audit_verdict : Prop :=
  ae_count_total = 18%nat /\
  ae_aligned_count = 6%nat /\
  ae_misaligned_count = 0%nat /\
  ae_missing_count = 0%nat /\
  ae_non_equivalence_count = 12%nat /\
  (ae_aligned_count + ae_misaligned_count
   + ae_missing_count + ae_non_equivalence_count)%nat = ae_count_total /\
  anti_equivalence_safety_framework_established = true /\
  audit_no_equivalence_claims /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma anti_equivalence_safety_audit_verdict_holds :
  anti_equivalence_safety_audit_verdict.
Proof.
  unfold anti_equivalence_safety_audit_verdict.
  split; [exact ae_count_total_value | ].
  split; [exact ae_aligned_count_value | ].
  split; [exact ae_misaligned_count_value | ].
  split; [exact ae_missing_count_value | ].
  split; [exact ae_non_equivalence_count_value | ].
  split; [exact audit_partition_complete | ].
  split; [exact anti_equivalence_safety_framework_established_value | ].
  split; [exact audit_no_equivalence_claims_holds | ].
  split; [exact audit_ae_pd_4f_ob_02_open | ].
  exact audit_ae_pd_4f_cpp_01_open.
Qed.
