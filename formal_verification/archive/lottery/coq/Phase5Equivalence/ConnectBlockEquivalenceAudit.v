(** Phase5Equivalence/ConnectBlockEquivalenceAudit.v
    ConnectBlock equivalence audit counts (TASK-018FF-E).

    Aggregates resolved / disagreement / missing / partial counts over the
    ConnectBlock catalog, marks the verdict PARTIAL because the projection
    encoder [trace_height] is an identity-style placeholder (block-height
    domain only) and cross-domain closure to the 256-bit block-hash payloads
    remains open. The encoder does NOT fabricate a hash from the height. *)

From Coq Require Import List Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalence.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalenceCatalog.

(** ---- Re-exported counts ---- *)

Definition cb_resolved : nat := cb_catalog_resolved_count.
Definition cb_disagreement : nat := cb_catalog_disagreement_count.
Definition cb_missing : nat := cb_catalog_missing_count.
Definition cb_partial : nat := cb_catalog_partial_count.

Lemma cb_resolved_value : cb_resolved = 5%nat.
Proof. exact cb_catalog_resolved_count_value. Qed.

Lemma cb_disagreement_value : cb_disagreement = 8%nat.
Proof. exact cb_catalog_disagreement_count_value. Qed.

Lemma cb_missing_value : cb_missing = 0%nat.
Proof. exact cb_catalog_missing_count_value. Qed.

Lemma cb_partial_value : cb_partial = 0%nat.
Proof. exact cb_catalog_partial_count_value. Qed.

(** ---- Partial verdict (identity projection + open cross-domain) ---- *)

Definition cb_equivalence_uses_identity_projection : bool :=
  cb_projection_uses_identity_map.

Definition cb_equivalence_marked_partial : bool :=
  cb_equivalence_uses_identity_projection &&
  negb cb_equivalence_cross_domain_closed.

Lemma cb_equivalence_uses_identity_projection_value :
  cb_equivalence_uses_identity_projection = true.
Proof. reflexivity. Qed.

Lemma cb_equivalence_marked_partial_value :
  cb_equivalence_marked_partial = true.
Proof. exact cb_equivalence_status_partial_value. Qed.

(** REQUIRED: projection layer recorded (frozen=runtime + 4 totality). *)
Definition cb_projection_layer_recorded : Prop :=
  cb_equivalence_projection_verified = true /\
  cb_catalog_resolved_count = 5%nat /\
  cb_catalog_count = 13%nat.

Lemma cb_projection_layer_recorded_holds :
  cb_projection_layer_recorded.
Proof.
  unfold cb_projection_layer_recorded.
  split; [| split].
  - reflexivity.
  - exact cb_catalog_resolved_count_value.
  - exact cb_catalog_count_value.
Qed.

(** REQUIRED: cross-domain bridge honestly open (8 disagreements). *)
Lemma cb_cross_domain_honestly_open :
  any_sample_height_matches_frozen = false /\
  any_sample_height_matches_runtime = false.
Proof.
  split.
  - exact any_sample_height_matches_frozen_false.
  - exact any_sample_height_matches_runtime_false.
Qed.

(** Encoder does NOT fabricate a 256-bit hash from a small height. *)
Lemma cb_encoder_does_not_fabricate_hash :
  cb_projection_fabricates_hash = false.
Proof. exact cb_projection_does_not_fabricate_hash. Qed.

(** Other Phase-5 components remain untouched. *)
Lemma cb_only_component_advanced :
  negb equivalence_claimed = true /\
  cb_equivalence_marked_partial = true.
Proof.
  split.
  - exact equivalence_not_claimed.
  - exact cb_equivalence_marked_partial_value.
Qed.

(** Master TASK-018FF-E verdict (PARTIAL — projection layer recorded only). *)
Definition cb_equivalence_verdict : Prop :=
  cb_projection_layer_recorded /\
  cb_equivalence_marked_partial = true /\
  cb_disagreement = 8%nat /\
  cb_missing = 0%nat /\
  cb_partial = 0%nat /\
  cb_projection_fabricates_hash = false.

Lemma cb_equivalence_verdict_holds : cb_equivalence_verdict.
Proof.
  unfold cb_equivalence_verdict.
  repeat split; eauto using cb_projection_layer_recorded_holds.
Qed.

(** Explicit: this is NOT full PD-4F-OB-02 / global equivalence. *)
Lemma cb_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.
