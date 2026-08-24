(** Phase5Equivalence/DrawEquivalenceAudit.v
    Draw equivalence audit counts (TASK-018FF-B).

    Aggregates resolved / disagreement / missing counts over the Draw catalog.
    Marks the overall Draw verdict PARTIAL when the identity projection wrapper
    is in use and the cross-domain bridge to TASK-003 remains open. *)

From Coq Require Import List Bool Arith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationAudit.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.DrawEquivalence.
Require Import Rigor.Phase5Equivalence.DrawEquivalenceCatalog.

(** ---- Catalog audit (re-export counts) ---- *)

Definition draw_resolved : nat := draw_catalog_resolved_count.
Definition draw_disagreement : nat := draw_catalog_disagreement_count.
Definition draw_missing : nat := draw_catalog_missing_count.

Lemma draw_resolved_value : draw_resolved = 169%nat.
Proof. exact draw_catalog_resolved_count_value. Qed.

Lemma draw_disagreement_value : draw_disagreement = 168%nat.
Proof. exact draw_catalog_disagreement_count_value. Qed.

Lemma draw_missing_value : draw_missing = 0%nat.
Proof. exact draw_catalog_missing_count_value. Qed.

(** ---- Partial verdict (identity projection + open cross-domain) ---- *)

Definition draw_equivalence_uses_identity_projection : bool :=
  draw_projection_uses_identity_map.

Definition draw_equivalence_marked_partial : bool :=
  draw_equivalence_uses_identity_projection &&
  negb draw_equivalence_cross_domain_closed.

Lemma draw_equivalence_uses_identity_projection_value :
  draw_equivalence_uses_identity_projection = true.
Proof. reflexivity. Qed.

Lemma draw_equivalence_marked_partial_value :
  draw_equivalence_marked_partial = true.
Proof. exact draw_equivalence_status_partial_value. Qed.

(** REQUIRED: projection layer verified (168 golden rows + frozen=runtime). *)
Definition draw_projection_layer_verified : Prop :=
  draw_equivalence_projection_verified = true /\
  draw_catalog_resolved_count = 169%nat /\
  representation_match_count = 168%nat.

Lemma draw_projection_layer_verified_holds : draw_projection_layer_verified.
Proof.
  unfold draw_projection_layer_verified.
  split; [| split].
  - reflexivity.
  - exact draw_catalog_resolved_count_value.
  - exact representation_match_count_value.
Qed.

(** REQUIRED: cross-domain bridge honestly open (0 golden matches TASK-003). *)
Lemma draw_cross_domain_honestly_open :
  any_golden_row_matches_frozen = false.
Proof. exact any_golden_row_matches_frozen_false. Qed.

(** Other Phase-5 components remain untouched (still pending in 018FF-A). *)
Lemma draw_only_component_advanced :
  negb equivalence_claimed = true /\
  draw_equivalence_marked_partial = true.
Proof.
  split.
  - exact equivalence_not_claimed.
  - exact draw_equivalence_marked_partial_value.
Qed.

(** Master TASK-018FF-B verdict (PARTIAL — Draw projection only). *)
Definition draw_equivalence_verdict : Prop :=
  draw_projection_layer_verified /\
  draw_equivalence_marked_partial = true /\
  draw_disagreement = 168%nat /\
  draw_missing = 0%nat.

Lemma draw_equivalence_verdict_holds : draw_equivalence_verdict.
Proof.
  unfold draw_equivalence_verdict.
  repeat split; eauto using draw_projection_layer_verified_holds.
Qed.

(** Explicit: this is NOT full PD-4F-OB-02 / global equivalence. *)
Lemma draw_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.
