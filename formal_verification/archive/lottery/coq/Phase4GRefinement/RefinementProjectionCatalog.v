(** Phase4GRefinement/RefinementProjectionCatalog.v
    Refinement projection catalog (PHASE-4G-006S).

    Maps every 005R gap record (one per 004Q disagreement) to exactly one
    projection record, tagged with the component's projection status. SPEC ONLY,
    no equivalence claim, no consensus/runtime/RFC/SHA256 code touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4GRefinement.RefinementGapCatalog.
Require Import Rigor.Phase4GRefinement.RefinementGapAudit.
Require Import Rigor.Phase4GRefinement.RefinementProjection.

(** One projection record per gap record (same component + kind, plus status). *)
Definition mk_projection (g : RefinementGapRecord) : ProjectionRecord :=
  {| proj_component := g.(rg_component);
     proj_gap_kind := g.(rg_kind);
     proj_status := projection_status_of g.(rg_component) |}.

(** REQUIRED-2: projection catalog built from the 005R gap catalog. *)
Definition projection_catalog : list ProjectionRecord := map mk_projection gap_catalog.

Definition projection_count : nat := length projection_catalog.

(** REQUIRED-2: catalog is non-empty. *)
Lemma projection_catalog_nonempty : projection_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** REQUIRED-2: every disagreement maps to exactly one projection record. *)
Lemma projection_count_matches_gap_count : projection_count = gap_count.
Proof.
  unfold projection_count, projection_catalog, gap_count.
  rewrite map_length. reflexivity.
Qed.
