(** Phase4GRefinement/Phase4FEndToEndReadinessCatalog.v
    Final readiness catalog: components x sources (PHASE-4F-008V).

    For each component, one entry per source (Frozen C++, Live Runtime, Semantic),
    each carrying the component's refinement projection status. Supply entries carry
    PS_Undefined. SPEC ONLY, no equivalence claim, no consensus/runtime/RFC/SHA256
    code touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.Phase4FEndToEndReadiness.

(** REQUIRED-2: the three comparison/observation sources. *)
Inductive ReadinessSource := RS_Frozen | RS_Runtime | RS_Semantic.

Record ReadinessCatalogEntry := {
  rce_component : BridgeComponent;
  rce_source : ReadinessSource;
  rce_projection : ProjectionStatus;
}.

Definition entries_of (r : ReadinessRecord) : list ReadinessCatalogEntry :=
  [ {| rce_component := r.(rr_component); rce_source := RS_Frozen;   rce_projection := r.(rr_projection) |};
    {| rce_component := r.(rr_component); rce_source := RS_Runtime;  rce_projection := r.(rr_projection) |};
    {| rce_component := r.(rr_component); rce_source := RS_Semantic; rce_projection := r.(rr_projection) |} ].

(** REQUIRED-2: all components x all sources. *)
Definition readiness_catalog : list ReadinessCatalogEntry :=
  flat_map entries_of readiness_records.

Definition readiness_catalog_count : nat := length readiness_catalog.

Definition source_eqb (a b : ReadinessSource) : bool :=
  match a, b with
  | RS_Frozen, RS_Frozen => true
  | RS_Runtime, RS_Runtime => true
  | RS_Semantic, RS_Semantic => true
  | _, _ => false
  end.

Definition source_count (s : ReadinessSource) : nat :=
  length (filter (fun e => source_eqb e.(rce_source) s) readiness_catalog).

(** REQUIRED-2: catalog is non-empty. *)
Lemma catalog_nonempty : readiness_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** REQUIRED-2: 5 components x 3 sources = 15 entries. *)
Lemma readiness_catalog_count_value : readiness_catalog_count = 15%nat.
Proof. vm_compute. reflexivity. Qed.

(** Each source contributes exactly one entry per component (5 each). *)
Lemma source_counts_balanced :
  source_count RS_Frozen = 5%nat /\
  source_count RS_Runtime = 5%nat /\
  source_count RS_Semantic = 5%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

(** REQUIRED-2: every catalog entry's component matches the readiness records
    (component-aligned mapping; the catalog invents no component). The expected
    sequence is each component repeated 3 times (one per source), in 004Q order. *)
Lemma all_entries_mapped :
  map rce_component readiness_catalog
    = [ BC_Draw; BC_Draw; BC_Draw;
        BC_Replay; BC_Replay; BC_Replay;
        BC_Reorg; BC_Reorg; BC_Reorg;
        BC_ConnectBlock; BC_ConnectBlock; BC_ConnectBlock;
        BC_Supply; BC_Supply; BC_Supply ].
Proof. vm_compute. reflexivity. Qed.
