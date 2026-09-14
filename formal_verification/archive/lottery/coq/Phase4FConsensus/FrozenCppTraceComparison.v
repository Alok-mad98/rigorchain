(** Phase4FConsensus/FrozenCppTraceComparison.v
    Coq-trace vs Frozen-C++ comparison layer (PHASE-4F-001L).

    Reuses the 001K comparison machinery to compare, per component
    (Draw / Replay / Reorg / Supply / ConnectBlock), each frozen C++ observation
    against the closest Coq-trace or runtime reference. AUDIT ONLY: MUST NOT claim
    equivalence, replay/consensus/runtime correctness, Coq = C++, or
    Coq = Frozen C++. No consensus / runtime / RFC / SHA256 / ConvergeRoot /
    ConnectBlock / mining / wallet code is touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.TraceObservationBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
(* 001M: live-runtime projection cross-check layer. *)
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.

(** Reuse the 001K 3-valued result (FAR_Agree | FAR_Disagree | FAR_Missing). *)
Definition FrozenTraceAgreementResult := FrozenAgreementResult.

(** REQUIRED-1: compare one frozen C++ observation to its Coq/runtime reference. *)
Definition compare_frozen_vs_coq_observation
  (o : FrozenCppObservation) (ref : FrozenReference) : FrozenTraceAgreementResult :=
  compare_frozen_observation o ref.

(** Per-component comparison helpers (Draw / Replay / Reorg / Supply / ConnectBlock).
    Each takes the frozen observation and its mapped reference. *)
Definition compare_draw (o : FrozenCppObservation) (ref : FrozenReference) :=
  compare_frozen_vs_coq_observation o ref.
Definition compare_replay (o : FrozenCppObservation) (ref : FrozenReference) :=
  compare_frozen_vs_coq_observation o ref.
Definition compare_reorg (o : FrozenCppObservation) (ref : FrozenReference) :=
  compare_frozen_vs_coq_observation o ref.
Definition compare_supply (o : FrozenCppObservation) (ref : FrozenReference) :=
  compare_frozen_vs_coq_observation o ref.
Definition compare_connectblock (o : FrozenCppObservation) (ref : FrozenReference) :=
  compare_frozen_vs_coq_observation o ref.

(** Counts (reuse the 001K tallies over a pair list). *)
Definition trace_agreement_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat := agreement_count ps.
Definition trace_disagreement_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat := disagreement_count ps.
Definition trace_missing_count
  (ps : list (FrozenCppObservation * FrozenReference)) : nat := missing_count ps.

(** Comparison reuse is faithful to the underlying bridge comparison. *)
Lemma compare_frozen_vs_coq_observation_sound :
  forall o ref,
    compare_frozen_vs_coq_observation o ref = compare_frozen_observation o ref.
Proof. intros o ref. reflexivity. Qed.

(** 001M cross-check: live-projected Coq trace vs frozen C++ observation.
    NOTE: agreements here are projection identities (import cross-consistency),
    NOT independent Coq-vs-C++ equivalence. *)
Definition compare_live_vs_frozen
  (l : LiveTraceObservation) (o : FrozenCppObservation) : FrozenTraceAgreementResult :=
  compare_live_vs_frozen_observation l o.
