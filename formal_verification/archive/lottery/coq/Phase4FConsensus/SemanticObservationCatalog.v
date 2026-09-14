(** Phase4FConsensus/SemanticObservationCatalog.v
    Catalog of Coq-derived semantic observations (PHASE-4F-001N).

    Entries are produced by the Phase-4F Coq formal model that realizes each
    phase's semantics:

      - Draw        (Phase-4A selection): Coq recompute_winner over candidate inputs
      - Replay      (Phase-4B):           deterministic trace re-execution
      - Reorg       (Phase-4B):           rollback transition in the Coq machine
      - ConnectBlock(Phase-4E):           accept-block transition in the Coq machine

    NO runtime evidence, NO frozen evidence, NO JSON parsing, NO external files.
    Candidate inputs come from the golden-vector data (test inputs to the model),
    but the observation CONTENT is computed by the Coq model, not imported. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.

(** Candidate inputs for the draw derivation (representative golden row WIN-001). *)
Definition sample_draw_candidates : list DrawCandidate :=
  match draw_audit_rows with
  | _ :: r1 :: _ => dar_candidates r1
  | r0 :: _ => dar_candidates r0
  | [] => nil
  end.

(** Coq-constructed traces (model inputs, not imported payloads). *)
Definition replay_trace : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3 ].

Definition reorg_trace : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3;
    TrAcceptBlock 4; TrAcceptBlock 5; TrRollback 2 ].

Definition sem_draw : SemanticObservation :=
  derive_draw_observation sample_draw_candidates.
Definition sem_replay : SemanticObservation :=
  derive_replay_observation init_consensus_state replay_trace.
Definition sem_reorg : SemanticObservation :=
  derive_reorg_observation init_consensus_state reorg_trace.
Definition sem_connectblock : SemanticObservation :=
  derive_connectblock_observation init_consensus_state 7.

(** REQUIRED-2: semantic observation catalog (all Coq-derived). *)
Definition semantic_observation_catalog : list SemanticObservation :=
  [ sem_draw; sem_replay; sem_reorg; sem_connectblock ].
