(** Phase4GRefinement/RefinementProjectionAudit.v
    Refinement projection audit (PHASE-4G-006S).

    Tallies the projection catalog by status:
      defined_projection_count   = 2  (Draw x {frozen, runtime})
      partial_projection_count   = 6  (Replay/Reorg/ConnectBlock x {frozen, runtime})
      undefined_projection_count = 0  (Supply is not a disagreement; see note below)

    Supply remains intentionally undefined at the component level
    (`supply_projection_undefined`) but produces NO gap record (it is Missing, not a
    disagreement), so it does not appear in this disagreement-derived catalog.

    SPEC ONLY — no equivalence claim, no consensus/runtime/RFC/SHA256 code touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4GRefinement.RefinementGapCatalog.
Require Import Rigor.Phase4GRefinement.RefinementGapAudit.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.RefinementProjectionCatalog.

Definition defined_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Defined) projection_catalog).
Definition partial_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Partial) projection_catalog).
Definition undefined_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Undefined) projection_catalog).

Lemma defined_projection_count_value : defined_projection_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma partial_projection_count_value : partial_projection_count = 6%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma undefined_projection_count_value : undefined_projection_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: the three status tallies partition the whole projection catalog. *)
Lemma projection_statistics_exist :
  defined_projection_count + partial_projection_count + undefined_projection_count
    = projection_count.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: at least one projection entry exists (the DEFINED Draw projection). *)
Lemma projection_entries_exist :
  exists p, In p projection_catalog /\ p.(proj_status) = PS_Defined.
Proof.
  eexists. split.
  - vm_compute. left. reflexivity.
  - vm_compute. reflexivity.
Qed.

(** REQUIRED-3: every gap record is mapped to a projection record, component-aligned. *)
Lemma all_gap_records_mapped :
  map proj_component projection_catalog = map rg_component gap_catalog.
Proof.
  unfold projection_catalog. rewrite map_map.
  apply map_ext. intro g. reflexivity.
Qed.
