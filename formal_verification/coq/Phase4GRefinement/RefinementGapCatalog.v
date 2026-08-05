(** Phase4GRefinement/RefinementGapCatalog.v
    Semantic-refinement gap catalog (PHASE-4G-005R).

    PURELY ANALYTICAL / AUDIT. For every DISAGREEMENT verdict discovered in the
    004Q end-to-end audit (Coq semantic observation vs frozen C++ / live runtime),
    this catalog records WHY the disagreement occurs (the missing refinement
    projection) and whether supplying that projection would be SAFE / UNSAFE /
    UNKNOWN. It does NOT attempt the projection and does NOT claim equivalence.

    No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock / mining /
    replay-execution code is touched. Records are derived from 004Q results. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.Phase4FEndToEndComparison.

(** REQUIRED-1: kinds of refinement gap that can cause a disagreement. *)
Inductive RefinementGapKind :=
| RGK_PayloadDomainMismatch       (* scalar model value vs 256-bit hash domain *)
| RGK_MissingSemanticProjection   (* model lacks the content needed to derive output *)
| RGK_MissingHashProjection       (* content exists, but no hash/txid mapping is defined *)
| RGK_MissingEligibilityProjection(* eligibility/sigma not modelled in Coq *)
| RGK_MissingRuntimeMapping        (* no mapping from model state to runtime record *)
| RGK_Unknown.

(** Safety of supplying the missing projection. *)
Inductive GapProjectionSafety :=
| GPS_Safe
| GPS_Unsafe
| GPS_Unknown.

(** REQUIRED-1: one gap record per disagreement. [rg_source]: 1 = frozen C++,
    2 = live runtime. *)
Record RefinementGapRecord := {
  rg_component : BridgeComponent;
  rg_source : nat;
  rg_kind : RefinementGapKind;
  rg_safety : GapProjectionSafety;
}.

(** Per-component root-cause classification.
    - Draw: a winner index exists; only the index->txid hash mapping is missing
      (DrawRepresentationBridge supplies it within the golden domain) => SAFE.
    - Replay/Reorg/ConnectBlock: the abstract Coq machine tracks only heights and
      has no block content to hash, so the semantic projection to a block/tip hash
      is missing; feasibility without full block reconstruction + SHA256 is UNKNOWN. *)
Definition classify_kind (c : BridgeComponent) : RefinementGapKind :=
  match c with
  | BC_Draw => RGK_MissingHashProjection
  | BC_Replay => RGK_MissingSemanticProjection
  | BC_Reorg => RGK_MissingSemanticProjection
  | BC_ConnectBlock => RGK_MissingSemanticProjection
  | _ => RGK_PayloadDomainMismatch
  end.

Definition classify_safety (c : BridgeComponent) : GapProjectionSafety :=
  match c with
  | BC_Draw => GPS_Safe
  | _ => GPS_Unknown
  end.

Definition mk_gap (c : BridgeComponent) (src : nat) : RefinementGapRecord :=
  {| rg_component := c;
     rg_source := src;
     rg_kind := classify_kind c;
     rg_safety := classify_safety c |}.

(** Emit a gap record for each disagreeing source of a 004Q per-component summary. *)
Definition gap_records_of (s : E2EComponentSummary) : list RefinementGapRecord :=
  (if far_is_disagree s.(e2e_vs_frozen) then [ mk_gap s.(e2e_component) 1 ] else [])
  ++
  (if far_is_disagree s.(e2e_vs_runtime) then [ mk_gap s.(e2e_component) 2 ] else []).

(** REQUIRED-1: the full refinement-gap catalog, derived from 004Q disagreements. *)
Definition gap_catalog : list RefinementGapRecord :=
  flat_map gap_records_of per_component_summary.

Definition gap_kind_eqb (a b : RefinementGapKind) : bool :=
  match a, b with
  | RGK_PayloadDomainMismatch, RGK_PayloadDomainMismatch => true
  | RGK_MissingSemanticProjection, RGK_MissingSemanticProjection => true
  | RGK_MissingHashProjection, RGK_MissingHashProjection => true
  | RGK_MissingEligibilityProjection, RGK_MissingEligibilityProjection => true
  | RGK_MissingRuntimeMapping, RGK_MissingRuntimeMapping => true
  | RGK_Unknown, RGK_Unknown => true
  | _, _ => false
  end.

Lemma gap_kind_eqb_refl : forall k, gap_kind_eqb k k = true.
Proof. intro k. destruct k; reflexivity. Qed.
