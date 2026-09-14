(** Phase4FConsensus/TraceObservationCatalog.v
    Catalog of Coq-trace observations (PHASE-4F-001H).

    Built ONLY from existing Phase-4F execution structures (draw audit rows,
    independent winner recomputation, projection/registry shape facts). No
    runtime evidence is read here. AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase4FConsensus.TraceObservationBridge.

(** Draw trace observations: one per audit row, carrying the INDEPENDENTLY
    recomputed winner index (a draw-domain index, not a txid). *)
Definition draw_trace_observations : list TraceObservationRecord :=
  map (fun r =>
        {| tor_kind := TOK_DrawWinner;
           tor_component := BC_Draw;
           tor_result := OR_Winner (IndependentWinnerSource r) |})
      draw_audit_rows.

(** A representative draw winner index (2nd row = WIN-001, winner 0) for pairing. *)
Definition draw_trace_winner : Z :=
  match draw_audit_rows with
  | _ :: r1 :: _ => IndependentWinnerSource r1
  | r0 :: _ => IndependentWinnerSource r0
  | [] => (-1)%Z
  end.

(** Projection observation: whether the draw projection had zero mismatches.
    Derived from the draw audit (Coq side only). *)
Definition projection_trace_observation : TraceObservationRecord :=
  {| tor_kind := TOK_Projection;
     tor_component := BC_Draw;
     tor_result := OR_Accept (Nat.eqb audit_mismatch_count 0) |}.

(** Registry execution observation: whether all generated trace cases are present.
    Shape fact about the Coq registry, NOT a runtime acceptance claim. *)
Definition registry_trace_observation : TraceObservationRecord :=
  {| tor_kind := TOK_RegistryExecution;
     tor_component := BC_RESRuntime;
     tor_result := OR_Accept (Nat.eqb (length all_registry_trace_cases)
                                      generated_trace_case_count) |}.

(** REQUIRED-2: full Coq-trace observation catalog. *)
Definition trace_observation_catalog : list TraceObservationRecord :=
  draw_trace_observations
    ++ [ projection_trace_observation; registry_trace_observation ].

Definition trace_observation_count : nat := length trace_observation_catalog.

(** One observation per draw audit row, plus the projection and registry observations. *)
Lemma trace_observation_count_value : trace_observation_count = 170%nat.
Proof.
  unfold trace_observation_count, trace_observation_catalog, draw_trace_observations.
  rewrite app_length.
  rewrite map_length.
  rewrite draw_audit_rows_length.
  unfold draw_audit_rows_count.
  reflexivity.
Qed.

(** REQUIRED-2: catalog is non-empty. *)
Lemma trace_catalog_nonempty : trace_observation_count <> 0%nat.
Proof.
  rewrite trace_observation_count_value. discriminate.
Qed.

