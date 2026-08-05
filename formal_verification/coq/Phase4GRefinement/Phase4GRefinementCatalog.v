(** Phase4GRefinement/Phase4GRefinementCatalog.v
    Phase-4G refinement closure catalog (PHASE-4G-007U).

    The consolidated gap-derived catalog: all 8 disagreement-driven projection
    records (006S/006T), each carrying its updated projection_status; Supply is
    marked undefined at the component level (it has no gap record). SPEC ONLY,
    no equivalence claim, no consensus/runtime/RFC/SHA256 code touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementGapCatalog.
Require Import Rigor.Phase4GRefinement.RefinementGapAudit.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.RefinementProjectionCatalog.

(** REQUIRED-2: the consolidated catalog = all 8 gap-derived projection records. *)
Definition refinement_catalog : list ProjectionRecord := projection_catalog.

Definition refinement_catalog_count : nat := length refinement_catalog.

(** REQUIRED-2: catalog is non-empty. *)
Lemma catalog_nonempty : refinement_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** REQUIRED-2: projection count matches the 005R gap count (= 8). *)
Lemma projection_count_matches_gap_count : refinement_catalog_count = gap_count.
Proof.
  unfold refinement_catalog_count, refinement_catalog, projection_catalog, gap_count.
  rewrite map_length. reflexivity.
Qed.

Lemma refinement_catalog_count_value : refinement_catalog_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-2: every gap record maps (component-aligned) to a projection record. *)
Lemma all_gap_records_mapped :
  map proj_component refinement_catalog = map rg_component gap_catalog.
Proof.
  unfold refinement_catalog, projection_catalog.
  rewrite map_map. apply map_ext. intro g. reflexivity.
Qed.

(** Supply is marked undefined (no gap record, undefined projection status). *)
Lemma supply_marked_undefined : projection_status_of BC_Supply = PS_Undefined.
Proof. reflexivity. Qed.
