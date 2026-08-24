(** Phase5Equivalence/SupplyEligibilityEquivalenceAudit.v
    Supply + Eligibility/sigma equivalence audit counts (TASK-018FF-F).

    Aggregates resolved / disagreement / missing / partial counts. Both
    projections are marked [PS_Partial]. Supply frozen/runtime payloads are
    MISSING; eligibility rows verify import-identity only. *)

From Coq Require Import List Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalence.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalenceCatalog.

(** ---- Re-exported counts ---- *)

Definition se_resolved : nat := se_catalog_resolved_count.
Definition se_disagreement : nat := se_catalog_disagreement_count.
Definition se_missing : nat := se_catalog_missing_count.
Definition se_partial : nat := se_catalog_partial_count.

Lemma se_resolved_value : se_resolved = 172%nat.
Proof. exact se_catalog_resolved_count_value. Qed.

Lemma se_disagreement_value : se_disagreement = 0%nat.
Proof. exact se_catalog_disagreement_count_value. Qed.

Lemma se_missing_value : se_missing = 2%nat.
Proof. exact se_catalog_missing_count_value. Qed.

Lemma se_partial_value : se_partial = 1%nat.
Proof. exact se_catalog_partial_count_value. Qed.

(** ---- PS_Partial markers ---- *)

Definition se_supply_projection_ps_partial : bool :=
  projection_status_partial_bool supply_projection_marked_partial.

Definition se_eligibility_projection_ps_partial : bool :=
  projection_status_partial_bool eligibility_projection_marked_partial.

Lemma se_supply_projection_ps_partial_value :
  se_supply_projection_ps_partial = true.
Proof. vm_compute. reflexivity. Qed.

Lemma se_eligibility_projection_ps_partial_value :
  se_eligibility_projection_ps_partial = true.
Proof. vm_compute. reflexivity. Qed.

Definition se_equivalence_marked_partial : bool :=
  supply_eligibility_status_partial.

Lemma se_equivalence_marked_partial_value :
  se_equivalence_marked_partial = true.
Proof. exact supply_eligibility_status_partial_value. Qed.

(** REQUIRED: projection layer recorded. *)
Definition se_projection_layer_recorded : Prop :=
  supply_eligibility_projection_verified = true /\
  se_catalog_count = 175%nat /\
  se_catalog_missing_count = 2%nat /\
  se_catalog_partial_count = 1%nat.

Lemma se_projection_layer_recorded_holds :
  se_projection_layer_recorded.
Proof.
  unfold se_projection_layer_recorded.
  split; [| split; [| split]].
  - reflexivity.
  - exact se_catalog_count_value.
  - exact se_catalog_missing_count_value.
  - exact se_catalog_partial_count_value.
Qed.

(** REQUIRED: eligibility identity verified for all 168 draw rows. *)
Lemma se_eligibility_identity_honest :
  draw_audit_row_count = 168%nat /\
  length eligibility_identity_catalog = 168%nat.
Proof.
  split; [exact draw_audit_row_count_value | exact eligibility_identity_catalog_count].
Qed.

(** REQUIRED: supply frozen payload honestly missing. *)
Lemma se_supply_frozen_honestly_missing :
  frozen_supply_present = false /\
  supply_frozen_side_result = SEER_Missing.
Proof. split; [exact frozen_supply_not_present | exact supply_frozen_side_is_missing]. Qed.

(** Other Phase-5 bridge components remain untouched. *)
Lemma se_only_supply_eligibility_advanced :
  negb equivalence_claimed = true /\
  se_equivalence_marked_partial = true.
Proof.
  split.
  - exact equivalence_not_claimed.
  - exact se_equivalence_marked_partial_value.
Qed.

(** Master TASK-018FF-F verdict (PARTIAL — projections verified, no equivalence). *)
Definition se_equivalence_verdict : Prop :=
  se_projection_layer_recorded /\
  se_equivalence_marked_partial = true /\
  se_supply_projection_ps_partial = true /\
  se_eligibility_projection_ps_partial = true /\
  se_disagreement = 0%nat /\
  supply_eligibility_equivalence_claimed = false.

Lemma se_equivalence_verdict_holds : se_equivalence_verdict.
Proof.
  unfold se_equivalence_verdict.
  repeat split; eauto using se_projection_layer_recorded_holds.
Qed.

(** Explicit: no global equivalence. *)
Lemma se_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.
