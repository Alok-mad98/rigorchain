(** Phase4GRefinement/Phase4GRefinementAudit.v
    Phase-4G refinement closure audit (PHASE-4G-007U).

    Final consolidated statistics over the gap-derived projection catalog:
      defined_projection_count   = 2  (Draw x {frozen, runtime})
      partial_projection_count   = 6  (Replay/Reorg/ConnectBlock x {frozen, runtime})
      undefined_projection_count = 0  (Supply is Missing, not a disagreement -> no entry)

    *** Circular dependencies / remaining proof debt ***
    - The Coq-side observations (001N) are independently derived (no projection
      identity), but the comparison payloads still disagree by domain (model values
      vs 256-bit hashes); the partial projections encode heights, not hashes.
    - Eligibility / sigma remain imported (PD-4F-OB-03).
    - PD-4F-OB-02 (Coq = runtime/frozen) and PD-4F-CPP-01 (Coq = frozen C++) remain
      OPEN / PARTIAL. This task establishes the projection closure FRAMEWORK only and
      claims NO equivalence (Coq != Runtime, != Frozen C++, != C++ are not asserted).

    SPEC ONLY. No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock /
    Replay / Reorg code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4GRefinement.RefinementGapCatalog.
Require Import Rigor.Phase4GRefinement.RefinementGapAudit.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.RefinementProjectionCatalog.
Require Import Rigor.Phase4GRefinement.Phase4GRefinementClosure.
Require Import Rigor.Phase4GRefinement.Phase4GRefinementCatalog.

Definition defined_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Defined) refinement_catalog).
Definition partial_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Partial) refinement_catalog).
Definition undefined_projection_count : nat :=
  length (filter (fun p => status_eqb p.(proj_status) PS_Undefined) refinement_catalog).

Lemma defined_projection_count_value : defined_projection_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma partial_projection_count_value : partial_projection_count = 6%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma undefined_projection_count_value : undefined_projection_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

(** The three catalog tallies partition the whole projection catalog. *)
Lemma final_statistics_partition :
  defined_projection_count + partial_projection_count + undefined_projection_count
    = refinement_catalog_count.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: every gap record is mapped to a projection record (count + alignment). *)
Lemma all_gap_records_mapped_to_projections :
  refinement_catalog_count = gap_count /\
  map proj_component refinement_catalog = map rg_component gap_catalog.
Proof.
  split.
  - exact projection_count_matches_gap_count.
  - exact all_gap_records_mapped.
Qed.

(** At least one defined projection entry exists (the Draw projection). *)
Lemma projection_entries_exist :
  exists p, In p refinement_catalog /\ p.(proj_status) = PS_Defined.
Proof.
  eexists. split.
  - vm_compute. left. reflexivity.
  - vm_compute. reflexivity.
Qed.

(** Per-component closure cross-check: 1 defined + 3 partial + 1 undefined = 5. *)
Lemma closure_component_total :
  closure_defined_count + closure_partial_count + closure_undefined_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.
