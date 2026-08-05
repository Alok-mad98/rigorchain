(** Phase5Equivalence/ReorgEquivalenceAudit.v
    Reorg equivalence audit counts (TASK-018FF-D).

    Aggregates resolved / disagreement / missing / partial counts over the
    Reorg catalog, marks the verdict PARTIAL because the projection encoder
    [trace_height] is an identity-style placeholder (post-rollback height
    domain only) and cross-domain closure to the 256-bit fork-hash payloads
    remains open. *)

From Coq Require Import List Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.ReorgEquivalence.
Require Import Rigor.Phase5Equivalence.ReorgEquivalenceCatalog.

(** ---- Re-exported counts ---- *)

Definition reorg_resolved : nat := reorg_catalog_resolved_count.
Definition reorg_disagreement : nat := reorg_catalog_disagreement_count.
Definition reorg_missing : nat := reorg_catalog_missing_count.
Definition reorg_partial : nat := reorg_catalog_partial_count.

Lemma reorg_resolved_value : reorg_resolved = 5%nat.
Proof. exact reorg_catalog_resolved_count_value. Qed.

Lemma reorg_disagreement_value : reorg_disagreement = 8%nat.
Proof. exact reorg_catalog_disagreement_count_value. Qed.

Lemma reorg_missing_value : reorg_missing = 0%nat.
Proof. exact reorg_catalog_missing_count_value. Qed.

Lemma reorg_partial_value : reorg_partial = 0%nat.
Proof. exact reorg_catalog_partial_count_value. Qed.

(** ---- Partial verdict (identity projection + open cross-domain) ---- *)

Definition reorg_equivalence_uses_identity_projection : bool :=
  reorg_projection_uses_identity_map.

Definition reorg_equivalence_marked_partial : bool :=
  reorg_equivalence_uses_identity_projection &&
  negb reorg_equivalence_cross_domain_closed.

Lemma reorg_equivalence_uses_identity_projection_value :
  reorg_equivalence_uses_identity_projection = true.
Proof. reflexivity. Qed.

Lemma reorg_equivalence_marked_partial_value :
  reorg_equivalence_marked_partial = true.
Proof. exact reorg_equivalence_status_partial_value. Qed.

(** REQUIRED: projection layer recorded (frozen=runtime identity + 4 totality). *)
Definition reorg_projection_layer_recorded : Prop :=
  reorg_equivalence_projection_verified = true /\
  reorg_catalog_resolved_count = 5%nat /\
  reorg_catalog_count = 13%nat.

Lemma reorg_projection_layer_recorded_holds :
  reorg_projection_layer_recorded.
Proof.
  unfold reorg_projection_layer_recorded.
  split; [| split].
  - reflexivity.
  - exact reorg_catalog_resolved_count_value.
  - exact reorg_catalog_count_value.
Qed.

(** REQUIRED: cross-domain bridge honestly open (8 disagreements). *)
Lemma reorg_cross_domain_honestly_open :
  any_sample_height_matches_frozen = false /\
  any_sample_height_matches_runtime = false.
Proof.
  split.
  - exact any_sample_height_matches_frozen_false.
  - exact any_sample_height_matches_runtime_false.
Qed.

(** Other Phase-5 components remain untouched. *)
Lemma reorg_only_component_advanced :
  negb equivalence_claimed = true /\
  reorg_equivalence_marked_partial = true.
Proof.
  split.
  - exact equivalence_not_claimed.
  - exact reorg_equivalence_marked_partial_value.
Qed.

(** Master TASK-018FF-D verdict (PARTIAL — projection layer recorded only). *)
Definition reorg_equivalence_verdict : Prop :=
  reorg_projection_layer_recorded /\
  reorg_equivalence_marked_partial = true /\
  reorg_disagreement = 8%nat /\
  reorg_missing = 0%nat /\
  reorg_partial = 0%nat.

Lemma reorg_equivalence_verdict_holds : reorg_equivalence_verdict.
Proof.
  unfold reorg_equivalence_verdict.
  repeat split; eauto using reorg_projection_layer_recorded_holds.
Qed.

(** Explicit: this is NOT full PD-4F-OB-02 / global equivalence. *)
Lemma reorg_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.
