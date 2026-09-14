(** Phase5Equivalence/CanonicalHashAlignmentAudit.v
    Canonical hash alignment audit (TASK-018GG-D).

    Aggregates the 18-cell alignment tally (6 components x 3 alignments per
    component) from CanonicalHashAlignmentBridge. Records that alignment
    DOES NOT imply equivalence, and that PD-4F-OB-02 / PD-4F-CPP-01 remain
    OPEN. *)

From Coq Require Import List Bool ZArith.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.

(** ---- Counts ---- *)

Definition alignment_count_total : nat := length all_alignments.

Definition aligned_count : nat :=
  length (filter ar_aligned all_alignments).
Definition misaligned_count : nat :=
  length (filter ar_misaligned all_alignments).
Definition missing_count : nat :=
  length (filter ar_missing all_alignments).

Lemma alignment_count_total_value : alignment_count_total = 18%nat.
Proof. exact all_alignments_length. Qed.

Lemma aligned_count_value : aligned_count = 6%nat.
Proof. exact all_alignments_aligned_count. Qed.

Lemma misaligned_count_value : misaligned_count = 12%nat.
Proof. exact all_alignments_misaligned_count. Qed.

Lemma missing_count_value : missing_count = 0%nat.
Proof. exact all_alignments_missing_count. Qed.

Lemma alignment_partition_complete :
  (aligned_count + misaligned_count + missing_count)%nat = alignment_count_total.
Proof. vm_compute. reflexivity. Qed.

(** ---- Structural-alignment honesty (NO equivalence implied) ---- *)

(** Even when alignment yields [AR_Aligned_Structurally] (only on the
    diagonal where source coincides with itself), this does NOT bridge to
    global equivalence: [equivalence_claimed] remains false, and the
    cross-source alignment functions never return aligned. *)
Definition alignment_does_not_imply_equivalence : Prop :=
  equivalence_claimed = false /\
  (forall a b, align_coq_vs_runtime a b <> AR_Aligned_Structurally) /\
  (forall a b, align_coq_vs_frozen a b <> AR_Aligned_Structurally) /\
  negb coq_eq_runtime_established = true /\
  negb coq_eq_frozen_established = true.

Lemma alignment_does_not_imply_equivalence_holds :
  alignment_does_not_imply_equivalence.
Proof.
  unfold alignment_does_not_imply_equivalence.
  split; [exact no_equivalence_leakage_global | ].
  split.
  { intros a b. apply (no_equivalence_leakage a b). }
  split.
  { intros a b. apply (no_equivalence_leakage a b). }
  split; [exact coq_eq_runtime_not_established |
          exact coq_eq_frozen_not_established].
Qed.

(** ---- Re-exports of the required obligations ---- *)

Lemma audit_alignment_deterministic :
  forall a1 a2 b1 b2 : HashObservation,
    a1 = a2 -> b1 = b2 ->
    align_self a1 b1 = align_self a2 b2 /\
    align_coq_vs_frozen a1 b1 = align_coq_vs_frozen a2 b2 /\
    align_coq_vs_runtime a1 b1 = align_coq_vs_runtime a2 b2.
Proof. exact alignment_deterministic. Qed.

Lemma audit_alignment_total :
  forall a b : HashObservation,
    (exists r, align_self a b = r) /\
    (exists r, align_coq_vs_frozen a b = r) /\
    (exists r, align_coq_vs_runtime a b = r).
Proof. exact alignment_total. Qed.

Lemma audit_no_equivalence_leakage :
  forall a b : HashObservation,
    align_coq_vs_runtime a b <> AR_Aligned_Structurally /\
    align_coq_vs_frozen a b <> AR_Aligned_Structurally.
Proof. exact no_equivalence_leakage. Qed.

Lemma audit_domain_gap_is_structural :
  AR_Misaligned_DomainGap <> AR_MissingProjection.
Proof. exact alignment_domain_gap_is_structural_not_failure. Qed.

(** ---- Honesty / debt re-exports ---- *)

Lemma audit_alignment_pd_4f_ob_02_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true.
Proof. exact canonical_hash_alignment_pd_ob_02_open. Qed.

Lemma audit_alignment_pd_4f_cpp_01_open :
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact canonical_hash_alignment_pd_cpp_01_open. Qed.

Lemma audit_alignment_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact canonical_hash_alignment_no_equivalence_claimed. Qed.

Lemma audit_alignment_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

(** ---- Master audit verdict ---- *)

Definition canonical_hash_alignment_audit_verdict : Prop :=
  alignment_count_total = 18%nat /\
  aligned_count = 6%nat /\
  misaligned_count = 12%nat /\
  missing_count = 0%nat /\
  (aligned_count + misaligned_count + missing_count)%nat = alignment_count_total /\
  canonical_hash_alignment_framework_established = true /\
  alignment_does_not_imply_equivalence /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma canonical_hash_alignment_audit_verdict_holds :
  canonical_hash_alignment_audit_verdict.
Proof.
  unfold canonical_hash_alignment_audit_verdict.
  split; [exact alignment_count_total_value | ].
  split; [exact aligned_count_value | ].
  split; [exact misaligned_count_value | ].
  split; [exact missing_count_value | ].
  split; [exact alignment_partition_complete | ].
  split; [exact canonical_hash_alignment_framework_established_value | ].
  split; [exact alignment_does_not_imply_equivalence_holds | ].
  split; [exact audit_alignment_pd_4f_ob_02_open | ].
  exact audit_alignment_pd_4f_cpp_01_open.
Qed.
