(** Phase5Equivalence/ReplayEquivalenceAudit.v
    Replay equivalence audit counts (TASK-018FF-C).

    Aggregates resolved / disagreement / missing / partial counts over the
    Replay catalog, marks the verdict PARTIAL because the projection encoder
    [trace_height] is an identity-style placeholder (height domain only) and
    cross-domain closure to the 256-bit hash payloads remains open. *)

From Coq Require Import List Bool.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.ReplayEquivalence.
Require Import Rigor.Phase5Equivalence.ReplayEquivalenceCatalog.

(** ---- Re-exported counts ---- *)

Definition replay_resolved : nat := replay_catalog_resolved_count.
Definition replay_disagreement : nat := replay_catalog_disagreement_count.
Definition replay_missing : nat := replay_catalog_missing_count.
Definition replay_partial : nat := replay_catalog_partial_count.

Lemma replay_resolved_value : replay_resolved = 5%nat.
Proof. exact replay_catalog_resolved_count_value. Qed.

Lemma replay_disagreement_value : replay_disagreement = 8%nat.
Proof. exact replay_catalog_disagreement_count_value. Qed.

Lemma replay_missing_value : replay_missing = 0%nat.
Proof. exact replay_catalog_missing_count_value. Qed.

Lemma replay_partial_value : replay_partial = 0%nat.
Proof. exact replay_catalog_partial_count_value. Qed.

(** ---- Partial verdict (identity projection + open cross-domain) ---- *)

Definition replay_equivalence_uses_identity_projection : bool :=
  replay_projection_uses_identity_map.

Definition replay_equivalence_marked_partial : bool :=
  replay_equivalence_uses_identity_projection &&
  negb replay_equivalence_cross_domain_closed.

Lemma replay_equivalence_uses_identity_projection_value :
  replay_equivalence_uses_identity_projection = true.
Proof. reflexivity. Qed.

Lemma replay_equivalence_marked_partial_value :
  replay_equivalence_marked_partial = true.
Proof. exact replay_equivalence_status_partial_value. Qed.

(** REQUIRED: projection layer recorded (frozen=runtime identity + 4 totality). *)
Definition replay_projection_layer_recorded : Prop :=
  replay_equivalence_projection_verified = true /\
  replay_catalog_resolved_count = 5%nat /\
  replay_catalog_count = 13%nat.

Lemma replay_projection_layer_recorded_holds :
  replay_projection_layer_recorded.
Proof.
  unfold replay_projection_layer_recorded.
  split; [| split].
  - reflexivity.
  - exact replay_catalog_resolved_count_value.
  - exact replay_catalog_count_value.
Qed.

(** REQUIRED: cross-domain bridge honestly open (8 disagreements). *)
Lemma replay_cross_domain_honestly_open :
  any_sample_height_matches_frozen = false /\
  any_sample_height_matches_runtime = false.
Proof.
  split.
  - exact any_sample_height_matches_frozen_false.
  - exact any_sample_height_matches_runtime_false.
Qed.

(** Other Phase-5 components remain untouched. *)
Lemma replay_only_component_advanced :
  negb equivalence_claimed = true /\
  replay_equivalence_marked_partial = true.
Proof.
  split.
  - exact equivalence_not_claimed.
  - exact replay_equivalence_marked_partial_value.
Qed.

(** Master TASK-018FF-C verdict (PARTIAL — projection layer recorded only). *)
Definition replay_equivalence_verdict : Prop :=
  replay_projection_layer_recorded /\
  replay_equivalence_marked_partial = true /\
  replay_disagreement = 8%nat /\
  replay_missing = 0%nat /\
  replay_partial = 0%nat.

Lemma replay_equivalence_verdict_holds : replay_equivalence_verdict.
Proof.
  unfold replay_equivalence_verdict.
  repeat split; eauto using replay_projection_layer_recorded_holds.
Qed.

(** Explicit: this is NOT full PD-4F-OB-02 / global equivalence. *)
Lemma replay_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.
