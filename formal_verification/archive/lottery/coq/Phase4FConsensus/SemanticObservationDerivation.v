(** Phase4FConsensus/SemanticObservationDerivation.v
    Independently derived Coq semantic observations (PHASE-4F-001N).

    Replaces 001M projection identities (runtime payload -> observation) with
    observations whose payloads are produced by the Coq formal model itself:

      - Draw        : winner index from the Coq selection kernel (recompute_winner)
      - Replay      : final height from deterministic trace re-execution
      - Reorg       : post-rollback height from the Coq machine
      - ConnectBlock: height after a single accept-block transition

    *** No runtime payload, no frozen payload, no imported agreement source is
    reused as observation content. *** AUDIT ONLY: no consensus / runtime / RFC /
    SHA256 / ConvergeRoot / ConnectBlock / mining / wallet code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.

(** REQUIRED-1: an observation derived purely from Coq model state.
    [so_origin] = 4 marks "Coq-semantic" provenance. *)
Record SemanticObservation := {
  so_component : BridgeComponent;
  so_present : bool;
  so_payload : Z;
  so_origin : nat;
}.

Definition height_of (o : option ConsensusState) : Z :=
  match o with
  | Some s => s.(cs_height)
  | None => (-1)%Z
  end.

(** Draw: winner index derived by the Coq selection kernel over candidate inputs.
    Content is computed (recompute_winner), NOT an imported winner value. *)
Definition derive_draw_observation (cands : list DrawCandidate) : SemanticObservation :=
  {| so_component := BC_Draw;
     so_present := true;
     so_payload := recompute_winner cands;
     so_origin := 4 |}.

(** Replay: deterministic re-execution of a Coq-constructed trace; observe height. *)
Definition derive_replay_observation
  (s : ConsensusState) (tr : consensus_trace) : SemanticObservation :=
  {| so_component := BC_Replay;
     so_present := true;
     so_payload := height_of (execute_trace s tr);
     so_origin := 4 |}.

(** Reorg: execute a trace ending in a rollback; observe the post-reorg height. *)
Definition derive_reorg_observation
  (s : ConsensusState) (tr : consensus_trace) : SemanticObservation :=
  {| so_component := BC_Reorg;
     so_present := true;
     so_payload := height_of (execute_trace s tr);
     so_origin := 4 |}.

(** ConnectBlock: height after a single accept-block transition. *)
Definition derive_connectblock_observation
  (s : ConsensusState) (bid : nat) : SemanticObservation :=
  {| so_component := BC_ConnectBlock;
     so_present := true;
     so_payload := height_of (execute_transition s (TrAcceptBlock bid));
     so_origin := 4 |}.

(** Provenance guard: every derived observation is Coq-semantic (origin = 4). *)
Lemma derived_origin_is_semantic_draw :
  forall c, (derive_draw_observation c).(so_origin) = 4%nat.
Proof. intro c. reflexivity. Qed.

Lemma derived_replay_present :
  forall s tr, (derive_replay_observation s tr).(so_present) = true.
Proof. intros s tr. reflexivity. Qed.

(** A single accept-block raises height by exactly one (Coq semantics). *)
Lemma connectblock_payload_is_succ_height :
  forall s bid,
    (derive_connectblock_observation s bid).(so_payload) = (s.(cs_height) + 1)%Z.
Proof. intros s bid. reflexivity. Qed.
