(** Phase4GRefinement/FinalReadinessCatalog.v
    Final readiness catalog (TASK-016EE).

    FREEZE/RE-EXPORT layer. This module does NOT introduce new content; it
    publishes the 015DD `EndToEndClosureCatalog` under stable "Final" names so
    external readers can refer to a single, immutable readiness surface. Every
    Qed below is discharged by an existing 015DD lemma — no new claim is made.

    AUDIT / SPEC ONLY. No consensus / runtime / RFC / SHA256 / ConvergeRoot /
    ConnectBlock / Replay / Reorg / mining code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.EndToEndClosureCatalog.

(** ================================================================
    Part A:  Final entry record (1:1 with [ClosureCatalogEntry])
    ================================================================ *)

(** Provenance tag carried by each final-readiness entry. The three sources
    coincide with [ClosureSource]; we keep a separate inductive to prevent
    accidental coupling with comparison-result types in audit code. *)
Inductive FinalProvenance := FP_FrozenCpp | FP_LiveRuntime | FP_CoqSemantic.

Definition provenance_of (s : ClosureSource) : FinalProvenance :=
  match s with
  | CS_Frozen => FP_FrozenCpp
  | CS_Runtime => FP_LiveRuntime
  | CS_Semantic => FP_CoqSemantic
  end.

Record FinalEntry := {
  fe_component : BridgeComponent;
  fe_source : ClosureSource;
  fe_provenance : FinalProvenance;
  fe_projection : ProjectionStatus;
  fe_circular : CircularDependencyKind;
}.

Definition mk_final_entry (e : ClosureCatalogEntry) : FinalEntry :=
  {| fe_component := e.(cce_component);
     fe_source := e.(cce_source);
     fe_provenance := provenance_of e.(cce_source);
     fe_projection := e.(cce_projection);
     fe_circular := e.(cce_circular) |}.

(** ================================================================
    Part B:  Final readiness catalog (re-export of 015DD closure_catalog)
    ================================================================ *)

Definition final_catalog : list FinalEntry :=
  map mk_final_entry closure_catalog.

Definition total_entry_count : nat := length final_catalog.

(** REQUIRED: catalog non-empty (re-export of 015DD `catalog_nonempty`). *)
Lemma catalog_nonempty : final_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** REQUIRED: 5 components x 3 sources = 15 entries. *)
Lemma total_entry_count_value : total_entry_count = 15%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED: every final entry's component matches the 004Q sequence (3 per
    component), inherited from [EndToEndClosureCatalog.all_entries_mapped]. *)
Lemma all_entries_mapped :
  map fe_component final_catalog
    = [ BC_Draw; BC_Draw; BC_Draw;
        BC_Replay; BC_Replay; BC_Replay;
        BC_Reorg; BC_Reorg; BC_Reorg;
        BC_ConnectBlock; BC_ConnectBlock; BC_ConnectBlock;
        BC_Supply; BC_Supply; BC_Supply ].
Proof. vm_compute. reflexivity. Qed.

(** Each Final entry's projection / circular status is the closure status of
    its component (re-export; no new content). *)
Lemma all_final_entries_projection_consistent :
  forall fe, In fe final_catalog ->
    fe.(fe_projection) = closure_projection_status_of fe.(fe_component).
Proof.
  intros fe Hin. unfold final_catalog in Hin.
  apply in_map_iff in Hin. destruct Hin as [e [Heq Hin]].
  subst fe. simpl. apply all_entries_projection_consistent. exact Hin.
Qed.

Lemma all_final_entries_circular_recorded :
  forall fe, In fe final_catalog ->
    fe.(fe_circular) = circular_of fe.(fe_component).
Proof.
  intros fe Hin. unfold final_catalog in Hin.
  apply in_map_iff in Hin. destruct Hin as [e [Heq Hin]].
  subst fe. simpl. apply all_entries_circular_recorded. exact Hin.
Qed.

(** Provenance is 1:1 with the underlying [ClosureSource]. *)
Lemma final_provenance_matches_source :
  forall fe, In fe final_catalog ->
    fe.(fe_provenance) = provenance_of fe.(fe_source).
Proof.
  intros fe Hin. unfold final_catalog in Hin.
  apply in_map_iff in Hin. destruct Hin as [e [Heq _]].
  subst fe. reflexivity.
Qed.
