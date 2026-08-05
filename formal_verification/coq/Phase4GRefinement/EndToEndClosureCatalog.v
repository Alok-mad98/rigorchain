(** Phase4GRefinement/EndToEndClosureCatalog.v
    End-to-end closure catalog (TASK-015DD).

    Consolidates ALL 5 consensus components (Draw, Replay, Reorg, ConnectBlock,
    Supply) across ALL 3 sources (Frozen C++, Live Runtime, Coq Semantic — the
    latter now INCLUDING the 014CC Supply + Eligibility/σ projections) into a
    single closure catalog. Each entry carries:
      - the component's closure-level refinement projection status, and
      - the explicit residual circular-dependency kind for that component.

    014CC integration: Supply's closure projection is now PS_Partial (was
    PS_Undefined), reflecting the typed Coq supply projection
    (`project_supply_after_trace`). Eligibility/σ is tracked separately (it is
    not a `BridgeComponent`) as a PS_Partial identity projection.

    AUDIT / SPEC ONLY. Does NOT claim Coq = Runtime / Coq = Frozen C++ / Coq =
    Consensus. No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock
    / Replay / Reorg / mining code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.

(** ================================================================
    Part A:  Closure sources and circular-dependency taxonomy
    ================================================================ *)

(** The three comparison/observation sources. *)
Inductive ClosureSource := CS_Frozen | CS_Runtime | CS_Semantic.

(** Residual circular-dependency / unresolved-dependency taxonomy. *)
Inductive CircularDependencyKind :=
| CDK_None                  (* independently Coq-derived; no payload reuse *)
| CDK_ImportedEligibility   (* draw input uses imported σ flags (PD-4F-OB-03) *)
| CDK_PayloadDomainGap      (* height vs 256-bit hash (resolved parametrically) *)
| CDK_SupplyAbsent.         (* supply absent on frozen+runtime; 0=0 identity *)

(** ================================================================
    Part B:  Closure-level projection status (integrates 014CC)
    ================================================================ *)

(** Closure projection status. Identical to [projection_status_of] EXCEPT that
    Supply is upgraded from PS_Undefined to PS_Partial (014CC), reflecting the
    now-existing Coq-side supply projection. Draw/Replay/Reorg/ConnectBlock are
    unchanged; the four equivalence obligations they map to remain CLOSED via
    011Y/012Z/013AA/014BB (parametric encoders). *)
Definition closure_projection_status_of (c : BridgeComponent) : ProjectionStatus :=
  match c with
  | BC_Draw => PS_Defined
  | BC_Replay => PS_Partial
  | BC_Reorg => PS_Partial
  | BC_ConnectBlock => PS_Partial
  | BC_Supply => PS_Partial          (* 014CC: was PS_Undefined *)
  | BC_RESRuntime => PS_Undefined
  end.

(** The closure status agrees with the pre-014CC status on every component
    except Supply, which it upgrades. *)
Lemma closure_status_supply_upgraded :
  projection_status_of BC_Supply = PS_Undefined /\
  closure_projection_status_of BC_Supply = PS_Partial.
Proof. split; reflexivity. Qed.

Lemma closure_status_unchanged_nonsupply :
  closure_projection_status_of BC_Draw = projection_status_of BC_Draw /\
  closure_projection_status_of BC_Replay = projection_status_of BC_Replay /\
  closure_projection_status_of BC_Reorg = projection_status_of BC_Reorg /\
  closure_projection_status_of BC_ConnectBlock = projection_status_of BC_ConnectBlock.
Proof. repeat split; reflexivity. Qed.

(** Per-component residual circular dependency. *)
Definition circular_of (c : BridgeComponent) : CircularDependencyKind :=
  match c with
  | BC_Draw => CDK_ImportedEligibility
  | BC_Replay => CDK_PayloadDomainGap
  | BC_Reorg => CDK_PayloadDomainGap
  | BC_ConnectBlock => CDK_PayloadDomainGap
  | BC_Supply => CDK_SupplyAbsent
  | BC_RESRuntime => CDK_None
  end.

(** ================================================================
    Part C:  Closure catalog entries (components x sources)
    ================================================================ *)

Record ClosureCatalogEntry := {
  cce_component : BridgeComponent;
  cce_source : ClosureSource;
  cce_projection : ProjectionStatus;
  cce_circular : CircularDependencyKind;
}.

(** The five consensus components in 004Q order. *)
Definition closure_components : list BridgeComponent :=
  [ BC_Draw; BC_Replay; BC_Reorg; BC_ConnectBlock; BC_Supply ].

Definition entries_of_component (c : BridgeComponent) : list ClosureCatalogEntry :=
  [ {| cce_component := c; cce_source := CS_Frozen;
       cce_projection := closure_projection_status_of c; cce_circular := circular_of c |};
    {| cce_component := c; cce_source := CS_Runtime;
       cce_projection := closure_projection_status_of c; cce_circular := circular_of c |};
    {| cce_component := c; cce_source := CS_Semantic;
       cce_projection := closure_projection_status_of c; cce_circular := circular_of c |} ].

(** REQUIRED: full closure catalog = 5 components x 3 sources. *)
Definition closure_catalog : list ClosureCatalogEntry :=
  flat_map entries_of_component closure_components.

Definition total_entry_count : nat := length closure_catalog.

(** ================================================================
    Part D:  Eligibility/σ closure entry (separate from BridgeComponent)
    ================================================================ *)

(** Eligibility/σ is not a [BridgeComponent]; its closure projection status is
    PS_Partial (014CC identity over imported flags). *)
Definition eligibility_closure_status : ProjectionStatus := PS_Partial.
Definition eligibility_closure_circular : CircularDependencyKind :=
  CDK_ImportedEligibility.

Lemma eligibility_closure_status_partial :
  eligibility_closure_status = PS_Partial.
Proof. reflexivity. Qed.

(** ================================================================
    Part E:  Catalog Qed proofs
    ================================================================ *)

(** Catalog is non-empty. *)
Lemma catalog_nonempty : closure_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

(** 5 components x 3 sources = 15 entries. *)
Lemma total_entry_count_value : total_entry_count = 15%nat.
Proof. vm_compute. reflexivity. Qed.

(** Every catalog entry's component matches the 004Q component sequence, each
    repeated once per source (catalog invents no component). *)
Lemma all_entries_mapped :
  map cce_component closure_catalog
    = [ BC_Draw; BC_Draw; BC_Draw;
        BC_Replay; BC_Replay; BC_Replay;
        BC_Reorg; BC_Reorg; BC_Reorg;
        BC_ConnectBlock; BC_ConnectBlock; BC_ConnectBlock;
        BC_Supply; BC_Supply; BC_Supply ].
Proof. vm_compute. reflexivity. Qed.

(** Every entry's projection status is exactly the closure status of its
    component (no fabricated status). *)
Lemma all_entries_projection_consistent :
  forall e, In e closure_catalog ->
    e.(cce_projection) = closure_projection_status_of e.(cce_component).
Proof.
  intros e Hin. unfold closure_catalog in Hin.
  apply in_flat_map in Hin. destruct Hin as [c [_ Hin]].
  unfold entries_of_component in Hin. simpl in Hin.
  destruct Hin as [H|[H|[H|H]]]; try (subst e; reflexivity); contradiction.
Qed.

(** Every entry records the explicit residual circular dependency of its
    component (all circular dependencies are recorded, none left implicit). *)
Lemma all_entries_circular_recorded :
  forall e, In e closure_catalog ->
    e.(cce_circular) = circular_of e.(cce_component).
Proof.
  intros e Hin. unfold closure_catalog in Hin.
  apply in_flat_map in Hin. destruct Hin as [c [_ Hin]].
  unfold entries_of_component in Hin. simpl in Hin.
  destruct Hin as [H|[H|[H|H]]]; try (subst e; reflexivity); contradiction.
Qed.
