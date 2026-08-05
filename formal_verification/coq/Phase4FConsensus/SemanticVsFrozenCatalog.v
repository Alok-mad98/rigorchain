(** Phase4FConsensus/SemanticVsFrozenCatalog.v
    Catalog of (Coq-semantic, Frozen-C++) comparison pairs (PHASE-4F-002O).

    Pairs each 001N Coq-derived semantic observation (origin = 4, Coq semantics)
    with the same-component frozen C++ observation (source = frozen C++, 001J).
    Supply has no independent Coq derivation in 001N, so its semantic side is an
    explicit absent placeholder (yields Missing — honest, not fabricated).

    NO runtime evidence, NO runtime projection, NO JSON, NO external files. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.

(** Supply: no 001N Coq-semantic observation exists; absent placeholder. *)
Definition sem_supply_absent : SemanticObservation :=
  {| so_component := BC_Supply;
     so_present := false;
     so_payload := 0;
     so_origin := 4 |}.

(** REQUIRED-2: catalog entries for Draw / Replay / Reorg / ConnectBlock, plus the
    honest Supply Missing entry. Left = Coq semantic (origin = 4); right = frozen C++. *)
Definition semantic_vs_frozen_pairs : list (SemanticObservation * FrozenCppObservation) :=
  [ (sem_draw, fco_draw);
    (sem_replay, fco_replay);
    (sem_reorg, fco_reorg);
    (sem_connectblock, fco_connectblock);
    (sem_supply_absent, fco_supply) ].

(** All left-hand observations carry Coq-semantic provenance (origin = 4). *)
Lemma semantic_pairs_origin_semantic :
  forallb (fun p => Nat.eqb (fst p).(so_origin) 4) semantic_vs_frozen_pairs = true.
Proof. reflexivity. Qed.
