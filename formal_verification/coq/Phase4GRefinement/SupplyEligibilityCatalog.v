(** Phase4GRefinement/SupplyEligibilityCatalog.v
    Supply + Eligibility/sigma observation catalog (TASK-014CC).

    Wraps the 014CC projections into Coq semantic observations and pairs them
    with the corresponding frozen / runtime sides. The Supply pair is a typed
    [0 = 0] structural identity (frozen absent, Coq machine init); the
    Eligibility/sigma catalog is a per-row import-identity of [dc_eligible].

    SPEC ONLY. No new oracle / bridge / readiness / status layer is created.
    No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock code
    modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeCatalog.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.

(** ================================================================
    Part A:  Coq-side Supply observation built from the projection
    ================================================================ *)

(** Coq supply observation derived from projecting the trace state. The payload
    is the projected supply (= 0 for any trace from init_consensus_state). *)
Definition derive_supply_observation (tr : consensus_trace) : SemanticObservation :=
  {| so_component := BC_Supply;
     so_present := true;
     so_payload := project_supply_after_trace tr;
     so_origin := 4 |}.

(** Concrete Supply observation for the Phase-4F replay trace (gives 0). *)
Definition sem_supply : SemanticObservation := derive_supply_observation replay_trace.

Lemma sem_supply_payload_zero : sem_supply.(so_payload) = 0%Z.
Proof. unfold sem_supply, derive_supply_observation. simpl.
       apply project_supply_after_trace_zero. Qed.

(** ================================================================
    Part B:  Supply pair entries for both comparison sides
    ================================================================ *)

(** Coq Supply observation paired with the (absent) frozen Supply observation —
    a structural [0 = 0] match by [supply_projection_structural_identity]. *)
Definition supply_frozen_pair : SemanticObservation * FrozenCppObservation :=
  (sem_supply, fco_supply).

(** Live runtime supply observation: there is no single runtime supply payload, so
    we use the explicit absent placeholder shared by 003P. *)
Definition sem_supply_runtime_obs : LiveTraceObservation := absent_live BC_Supply.

Definition supply_runtime_pair : SemanticObservation * LiveTraceObservation :=
  (sem_supply, sem_supply_runtime_obs).

(** ================================================================
    Part C:  Updated semantic-vs-frozen / semantic-vs-runtime catalogs
    ================================================================ *)

(** Replace the absent Supply entry from 002O with the projected Supply entry. *)
Definition supply_eligibility_semantic_vs_frozen_pairs
  : list (SemanticObservation * FrozenCppObservation) :=
  [ (sem_draw, fco_draw);
    (sem_replay, fco_replay);
    (sem_reorg, fco_reorg);
    (sem_connectblock, fco_connectblock);
    supply_frozen_pair ].

(** Replace the absent Supply entry from 003P with the projected Supply entry. *)
Definition supply_eligibility_semantic_vs_runtime_pairs
  : list (SemanticObservation * LiveTraceObservation) :=
  [ (sem_draw, project_live_runtime_to_coq_trace ev_003_winner);
    (sem_replay, project_live_runtime_to_coq_trace ev_005_multinode);
    (sem_reorg, project_live_runtime_to_coq_trace ev_005_reorg);
    (sem_connectblock, project_live_runtime_to_coq_trace ev_005_connect);
    supply_runtime_pair ].

(** All left-hand observations carry Coq-semantic provenance (origin = 4). *)
Lemma supply_pairs_origin_semantic_frozen :
  forallb (fun p => Nat.eqb (fst p).(so_origin) 4)
          supply_eligibility_semantic_vs_frozen_pairs = true.
Proof. reflexivity. Qed.

Lemma supply_pairs_origin_semantic_runtime :
  forallb (fun p => Nat.eqb (fst p).(so_origin) 4)
          supply_eligibility_semantic_vs_runtime_pairs = true.
Proof. reflexivity. Qed.

(** ================================================================
    Part D:  Eligibility / sigma per-row catalog
    ================================================================ *)

Record EligibilityCatalogEntry := {
  ece_vector_id : nat;
  ece_imported_flags : list bool;     (* the imported [dc_eligible] flags *)
  ece_projected_flags : list bool;    (* Coq-side projection (== imported) *)
  ece_eligible_count : nat;
}.

Definition build_eligibility_entry (r : DrawAuditRow) : EligibilityCatalogEntry :=
  {| ece_vector_id := dar_vector_id r;
     ece_imported_flags := map dc_eligible (dar_candidates r);
     ece_projected_flags := project_eligibility_sigma (dar_candidates r);
     ece_eligible_count := project_eligibility_count (dar_candidates r) |}.

Definition eligibility_catalog : list EligibilityCatalogEntry :=
  map build_eligibility_entry draw_audit_rows.

(** Identity invariant: imported flags = projected flags for every catalog row. *)
Lemma eligibility_catalog_identity :
  forall e, In e eligibility_catalog ->
    e.(ece_imported_flags) = e.(ece_projected_flags).
Proof.
  intros e Hin. apply in_map_iff in Hin. destruct Hin as [r [Heq _]].
  subst e. simpl. apply eligibility_projection_is_identity_map.
Qed.

(** Catalog covers the 14 draw-applicable rows. *)
Lemma eligibility_catalog_length : length eligibility_catalog = 14%nat.
Proof. vm_compute. reflexivity. Qed.
