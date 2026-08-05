(** Phase4GRefinement/Phase4GRefinementClosure.v
    Phase-4G refinement closure — per-component consolidation (PHASE-4G-007U).

    Consolidates the Phase-4G projections into one per-component record covering all
    five components (Draw / Replay / Reorg / ConnectBlock / Supply), with projection
    status, Coq-semantic origin, and a note. SPEC ONLY / REFINEMENT ONLY — does NOT
    claim Coq = Runtime / Coq = Frozen C++ / Coq = C++. No consensus / runtime / RFC /
    SHA256 / ConvergeRoot / ConnectBlock / Replay / Reorg code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4GRefinement.RefinementProjection.

(** Note attached to each closure record (provenance of the projection state). *)
Inductive ClosureNote :=
| CN_DefinedHashProjection    (* Draw: index -> txid (001I), total *)
| CN_PartialTraceHeight       (* Replay/Reorg/ConnectBlock: Some (trace_height) *)
| CN_UndefinedNoObservation.  (* Supply: no Coq-semantic observation *)

(** REQUIRED-1: per-component refinement closure record. *)
Record RefinementClosureRecord := {
  rc_component : BridgeComponent;
  rc_status : ProjectionStatus;
  rc_origin : nat;          (* 4 = Coq semantics *)
  rc_note : ClosureNote;
}.

Definition closure_note_of (c : BridgeComponent) : ClosureNote :=
  match c with
  | BC_Draw => CN_DefinedHashProjection
  | BC_Replay => CN_PartialTraceHeight
  | BC_Reorg => CN_PartialTraceHeight
  | BC_ConnectBlock => CN_PartialTraceHeight
  | _ => CN_UndefinedNoObservation
  end.

Definition mk_closure (c : BridgeComponent) : RefinementClosureRecord :=
  {| rc_component := c;
     rc_status := projection_status_of c;
     rc_origin := 4;
     rc_note := closure_note_of c |}.

(** All five components covered by the closure. *)
Definition closure_components : list BridgeComponent :=
  [ BC_Draw; BC_Replay; BC_Reorg; BC_ConnectBlock; BC_Supply ].

Definition closure_records : list RefinementClosureRecord :=
  map mk_closure closure_components.

(** Per-component status tallies (5 components). *)
Definition closure_defined_count : nat :=
  length (filter (fun r => status_eqb r.(rc_status) PS_Defined) closure_records).
Definition closure_partial_count : nat :=
  length (filter (fun r => status_eqb r.(rc_status) PS_Partial) closure_records).
Definition closure_undefined_count : nat :=
  length (filter (fun r => status_eqb r.(rc_status) PS_Undefined) closure_records).

Lemma closure_defined_count_value : closure_defined_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma closure_partial_count_value : closure_partial_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.
Lemma closure_undefined_count_value : closure_undefined_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-1: the per-component tallies partition all five closure records. *)
Lemma closure_statistics_exist :
  closure_defined_count + closure_partial_count + closure_undefined_count
    = length closure_records.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-1: each closure record's status is exactly the component's projection
    status (the consolidation introduces no inconsistency). *)
Lemma per_component_closure_consistency :
  forall r, In r closure_records ->
    r.(rc_status) = projection_status_of r.(rc_component).
Proof.
  intros r Hin. unfold closure_records in Hin.
  apply in_map_iff in Hin. destruct Hin as [c [Heq _]].
  subst r. reflexivity.
Qed.

(** Supply is consolidated as intentionally undefined. *)
Lemma supply_closure_undefined :
  projection_status_of BC_Supply = PS_Undefined.
Proof. reflexivity. Qed.
