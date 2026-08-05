(** Phase4GRefinement/RefinementProjection.v
    Refinement projection framework (PHASE-4G-006S).

    SPECIFICATION ONLY. Turns the 005R disagreement classifications into explicit
    refinement-projection hooks (Coq model value -> runtime/frozen payload domain),
    each tagged with how complete it is. This does NOT prove Coq = Runtime / Coq =
    Frozen C++; it only replaces "unexplained disagreement" with "named projection".

    No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock code is
    modified or implemented. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.DrawRepresentationBridge.
Require Import Rigor.Phase4GRefinement.RefinementGapCatalog.

(** REQUIRED-1: completeness of a refinement projection. *)
Inductive ProjectionStatus :=
| PS_Defined
| PS_Partial
| PS_Undefined.

(** REQUIRED-1: a projection record for one component/gap. *)
Record ProjectionRecord := {
  proj_component : BridgeComponent;
  proj_gap_kind : RefinementGapKind;
  proj_status : ProjectionStatus;
}.

(** ---- Projection stubs (REQUIRED-1) ----

    Draw: DEFINED — the index -> txid projection already exists (001I
    [winner_index_to_txid]) and is total within the golden candidate domain. *)
Definition project_draw (cands : list DrawCandidate) (winner_index : Z) : Z :=
  winner_index_to_txid cands winner_index.

(** Replay / Reorg / ConnectBlock: PARTIAL but now ACTIVE (006T). The projections
    are no longer inert [None]; they encode a value CONSISTENT WITH TRACE SEMANTICS
    — the final height reached by deterministically executing the trace through the
    Coq [ConsensusMachine]. This is a semantic placeholder sufficient for
    machine-auditable agreement counting; it is NOT the runtime/frozen block hash
    (deriving that still needs full block content + the hash kernel, out of scope),
    so the status stays PS_Partial and NO hash is fabricated. *)

(** Final height reached by replaying a trace from the initial state. *)
Definition trace_height (tr : consensus_trace) : Z :=
  match execute_trace init_consensus_state tr with
  | Some s => s.(cs_height)
  | None => (-1)%Z
  end.

(** Replay: semantic projection = replayed final height. *)
Definition project_replay (tr : consensus_trace) : option Z := Some (trace_height tr).

(** Reorg: semantic projection = post-reorg final height of the trace. *)
Definition project_reorg (tr : consensus_trace) : option Z := Some (trace_height tr).

(** ConnectBlock: semantic projection = height after the accept-block trace. *)
Definition project_connectblock (tr : consensus_trace) : option Z := Some (trace_height tr).

(** Supply: intentionally UNDEFINED — no Coq-semantic supply observation exists,
    so no projection function is provided. *)

(** Per-component projection status. *)
Definition projection_status_of (c : BridgeComponent) : ProjectionStatus :=
  match c with
  | BC_Draw => PS_Defined
  | BC_Replay => PS_Partial
  | BC_Reorg => PS_Partial
  | BC_ConnectBlock => PS_Partial
  | _ => PS_Undefined
  end.

Definition status_eqb (a b : ProjectionStatus) : bool :=
  match a, b with
  | PS_Defined, PS_Defined => true
  | PS_Partial, PS_Partial => true
  | PS_Undefined, PS_Undefined => true
  | _, _ => false
  end.

Lemma status_eqb_refl : forall s, status_eqb s s = true.
Proof. intro s. destruct s; reflexivity. Qed.

Lemma draw_projection_defined : projection_status_of BC_Draw = PS_Defined.
Proof. reflexivity. Qed.

Lemma supply_projection_undefined : projection_status_of BC_Supply = PS_Undefined.
Proof. reflexivity. Qed.

(** The Draw projection is total: it always yields a txid value (no [None]). *)
Lemma project_draw_total :
  forall cands idx, exists t, project_draw cands idx = t.
Proof. intros cands idx. eexists. reflexivity. Qed.

(** ---- 006T: the partial projections are now ACTIVE (return [Some], not [None]) ----
    These witness that Replay/Reorg/ConnectBlock projections are defined (a semantic
    height encoding), while remaining PS_Partial (not the runtime/frozen hash). *)

Lemma project_replay_partial :
  forall tr, exists z, project_replay tr = Some z /\ projection_status_of BC_Replay = PS_Partial.
Proof. intro tr. exists (trace_height tr). split; reflexivity. Qed.

Lemma project_reorg_partial :
  forall tr, exists z, project_reorg tr = Some z /\ projection_status_of BC_Reorg = PS_Partial.
Proof. intro tr. exists (trace_height tr). split; reflexivity. Qed.

Lemma project_connectblock_partial :
  forall tr, exists z, project_connectblock tr = Some z /\ projection_status_of BC_ConnectBlock = PS_Partial.
Proof. intro tr. exists (trace_height tr). split; reflexivity. Qed.
