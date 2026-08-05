(** Phase4FConsensus/SemanticVsLiveRuntimeCatalog.v
    Catalog of (Coq-semantic, Live-runtime) comparison pairs (PHASE-4F-003P).

    Pairs each 001N Coq-derived semantic observation (origin = 4, Coq semantics)
    with the same-component live runtime observation (source = runtime, projected
    in 001M from TASK-003/004/005 evidence). Supply has no independent Coq
    derivation in 001N, so its semantic side is an explicit absent placeholder
    (yields Missing — honest, not fabricated).

    NO Frozen C++ observation is reused as content, NO JSON, NO external files. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.

(** Supply: no 001N Coq-semantic observation exists; absent placeholder. *)
Definition sem_supply_absent_r : SemanticObservation :=
  {| so_component := BC_Supply;
     so_present := false;
     so_payload := 0;
     so_origin := 4 |}.

(** REQUIRED-2: catalog entries for Draw / Replay / Reorg / ConnectBlock, plus the
    honest Supply Missing entry. Left = Coq semantic (origin = 4); right = live
    runtime observation (source = runtime). *)
Definition semantic_vs_runtime_pairs : list (SemanticObservation * LiveTraceObservation) :=
  [ (sem_draw, project_live_runtime_to_coq_trace ev_003_winner);
    (sem_replay, project_live_runtime_to_coq_trace ev_005_multinode);
    (sem_reorg, project_live_runtime_to_coq_trace ev_005_reorg);
    (sem_connectblock, project_live_runtime_to_coq_trace ev_005_connect);
    (sem_supply_absent_r, absent_live BC_Supply) ].

(** All left-hand observations carry Coq-semantic provenance (origin = 4). *)
Lemma semantic_runtime_pairs_origin_semantic :
  forallb (fun p => Nat.eqb (fst p).(so_origin) 4) semantic_vs_runtime_pairs = true.
Proof. reflexivity. Qed.
