(** Phase4GRefinement/SupplyEligibilityAudit.v
    Supply + Eligibility/sigma audit (TASK-014CC).

    Computes machine-verifiable agreement / disagreement / missing counts for
    the projected Supply pair and the per-row Eligibility/sigma catalog.

    Honesty:
    - Supply: the Coq projection is 0 (init machine, no minting). The frozen
      side is absent (payload 0). The pair therefore agrees on a [0 = 0]
      structural identity — this is a typed-projection identity, NOT a real
      supply-equivalence claim.
    - Eligibility/sigma: the projection is identity over the imported
      [dc_eligible] flags; agreement here means the Coq projection literally
      reproduces the imported boolean list. It does NOT establish that Coq
      independently derives eligibility from the hash kernel and sigma
      threshold (PD-4F-OB-03 remains as documented).

    *** Coq = Runtime / Coq = Frozen C++ equivalence is NOT claimed. *** *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityCatalog.

(** ================================================================
    Part A:  Supply pair audit (vs frozen / vs runtime)
    ================================================================ *)

(** Supply-vs-frozen comparison result. *)
Definition supply_vs_frozen_result : SemanticFrozenAgreementResult :=
  compare_semantic_vs_frozen_observation sem_supply fco_supply.

Lemma supply_vs_frozen_is_missing :
  supply_vs_frozen_result = FAR_Missing.
Proof. reflexivity. Qed.

(** Supply-vs-runtime comparison result. *)
Definition supply_vs_runtime_result : FrozenAgreementResult :=
  compare_semantic_vs_live_runtime_observation sem_supply sem_supply_runtime_obs.

Lemma supply_vs_runtime_is_missing :
  supply_vs_runtime_result = FAR_Missing.
Proof. reflexivity. Qed.

(** Honest accounting: although Supply now has a Coq-side projection, the frozen
    and runtime sides are still absent, so the comparison result remains
    [FAR_Missing]. The added value of 014CC is the structural identity proven
    by [supply_projection_structural_identity], not a fresh agreement count. *)

(** ================================================================
    Part B:  Updated full pair lists and per-source counts
    ================================================================ *)

Definition full_sf_results : list FrozenAgreementResult :=
  semantic_frozen_results supply_eligibility_semantic_vs_frozen_pairs.

Definition full_sr_results : list FrozenAgreementResult :=
  semantic_runtime_results supply_eligibility_semantic_vs_runtime_pairs.

Definition full_sf_agreement_count : nat :=
  length (filter far_is_agree full_sf_results).
Definition full_sf_disagreement_count : nat :=
  length (filter far_is_disagree full_sf_results).
Definition full_sf_missing_count : nat :=
  length (filter far_is_missing full_sf_results).

Definition full_sr_agreement_count : nat :=
  length (filter far_is_agree full_sr_results).
Definition full_sr_disagreement_count : nat :=
  length (filter far_is_disagree full_sr_results).
Definition full_sr_missing_count : nat :=
  length (filter far_is_missing full_sr_results).

(** Counts (machine-verifiable). The 4 raw-payload disagreements (Draw / Replay
    / Reorg / ConnectBlock) remain on each side, plus 1 Missing for Supply
    (frozen side absent / runtime side absent). *)
Lemma full_sf_agreement_count_value :
  full_sf_agreement_count = 0%nat.
Proof. reflexivity. Qed.

Lemma full_sf_disagreement_count_value :
  full_sf_disagreement_count = 4%nat.
Proof. reflexivity. Qed.

Lemma full_sf_missing_count_value :
  full_sf_missing_count = 1%nat.
Proof. reflexivity. Qed.

Lemma full_sr_agreement_count_value :
  full_sr_agreement_count = 0%nat.
Proof. reflexivity. Qed.

Lemma full_sr_disagreement_count_value :
  full_sr_disagreement_count = 4%nat.
Proof. reflexivity. Qed.

Lemma full_sr_missing_count_value :
  full_sr_missing_count = 1%nat.
Proof. reflexivity. Qed.

(** Total (frozen + runtime) end-to-end raw counts. These match the 008V counts
    (0 / 8 / 2). 014CC adds projection identities and structural soundness
    lemmas without altering the raw payload-comparison counts. *)
Definition e2e_total_agreement : nat :=
  full_sf_agreement_count + full_sr_agreement_count.
Definition e2e_total_disagreement : nat :=
  full_sf_disagreement_count + full_sr_disagreement_count.
Definition e2e_total_missing : nat :=
  full_sf_missing_count + full_sr_missing_count.

Lemma e2e_total_agreement_value : e2e_total_agreement = 0%nat.
Proof. reflexivity. Qed.

Lemma e2e_total_disagreement_value : e2e_total_disagreement = 8%nat.
Proof. reflexivity. Qed.

Lemma e2e_total_missing_value : e2e_total_missing = 2%nat.
Proof. reflexivity. Qed.

(** ================================================================
    Part C:  Eligibility/sigma per-row audit
    ================================================================ *)

(** Number of catalog rows = number of imported draw rows = 14. *)
Definition eligibility_audit_total : nat := length eligibility_catalog.

Lemma eligibility_audit_total_value :
  eligibility_audit_total = 168%nat.
Proof. apply eligibility_catalog_length. Qed.

(** Every catalog row has projected flags equal to imported flags. *)
Definition all_eligibility_rows_identity : bool :=
  forallb
    (fun e =>
       if list_eq_dec Bool.bool_dec
            e.(ece_imported_flags) e.(ece_projected_flags)
       then true else false)
    eligibility_catalog.

Lemma all_eligibility_rows_identity_value :
  all_eligibility_rows_identity = true.
Proof. vm_compute. reflexivity. Qed.

(** Total eligible-candidate count across every audited row. *)
Definition eligibility_total_eligible_count : nat :=
  fold_right Nat.add 0 (map ece_eligible_count eligibility_catalog).

Lemma eligibility_total_eligible_count_value :
  eligibility_total_eligible_count = 628%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part D:  Updated projection statuses for Supply / Eligibility
    ================================================================ *)

(** Both Supply and Eligibility/sigma move from PS_Undefined / PS_Imported to
    PS_Partial after 014CC. They are not PS_Defined because:
      - Supply: no Coq minting model (cs_supply_oru / cs_supply_res are static).
      - Eligibility: no Coq re-derivation (still imported via dc_eligible). *)
Definition supply_status_after_014CC : ProjectionStatus :=
  supply_projection_status.
Definition eligibility_status_after_014CC : ProjectionStatus :=
  eligibility_projection_status.

Lemma supply_status_partial :
  supply_status_after_014CC = PS_Partial.
Proof. reflexivity. Qed.

Lemma eligibility_status_partial :
  eligibility_status_after_014CC = PS_Partial.
Proof. reflexivity. Qed.

(** ================================================================
    Part E:  Closing summary
    ================================================================ *)

(** Audit-level summary: raw payload comparison counts unchanged (0/8/2), but
    Supply and Eligibility/sigma both have new identity-faithful Coq-side
    projections. Coq = Runtime / Coq = Frozen C++ remains unproven. *)
Lemma audit_counts_match_phase4f_e2e :
  e2e_total_agreement = 0%nat /\
  e2e_total_disagreement = 8%nat /\
  e2e_total_missing = 2%nat.
Proof.
  split; [ reflexivity | split; reflexivity ].
Qed.

Lemma supply_and_eligibility_partial_status_after_014CC :
  supply_status_after_014CC = PS_Partial /\
  eligibility_status_after_014CC = PS_Partial.
Proof. split; reflexivity. Qed.

Lemma eligibility_catalog_full_identity :
  all_eligibility_rows_identity = true /\
  eligibility_audit_total = 168%nat.
Proof. split; [ apply all_eligibility_rows_identity_value
              | apply eligibility_audit_total_value ]. Qed.
