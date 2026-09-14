(** Phase4FConsensus/FrozenCppTraceCatalog.v
    Mapping: frozen C++ observation -> closest Coq-trace/runtime reference (001L).

    Draw            -> Coq trace (winner_index -> winner_txid via DrawRepresentation)
    Replay/Reorg/ConnectBlock -> RuntimeObservation (frozen block/fork/tip hashes)
    Supply          -> None

    No new observations, no external input, no JSON parsing, no runtime calls. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationAudit.
(* 001M: live-runtime projected observations for cross-checking. *)
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.

(** Draw reference: a Coq-trace winner txid obtained by the winner_index ->
    winner_txid projection (DrawRepresentation). Representative entry = WIN-001
    (Coq index 0 -> candidate-0 txid). This is the golden draw domain, NOT the
    live C++ block winner, so the Draw comparison is expected to DISAGREE. *)
Definition coq_draw_trace_txid : Z :=
  match representation_catalog with
  | _ :: e1 :: _ => entry_coq_txid e1
  | e0 :: _ => entry_coq_txid e0
  | [] => 0
  end.

Definition tref_draw : FrozenReference :=
  {| fr_present := true; fr_payload := coq_draw_trace_txid; fr_source := 1 |}.

(** Runtime references (frozen block/fork/tip hashes). *)
Definition tref_connectblock : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_connect; fr_source := 2 |}.
Definition tref_reorg : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_reorg; fr_source := 2 |}.
Definition tref_replay : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_multinode; fr_source := 2 |}.

Definition tref_none : FrozenReference :=
  {| fr_present := false; fr_payload := 0; fr_source := 0 |}.

(** REQUIRED-2: per-component frozen-C++ -> reference mapping. *)
Definition frozen_trace_pairs : list (FrozenCppObservation * FrozenReference) :=
  [ (fco_draw, tref_draw);
    (fco_replay, tref_replay);
    (fco_reorg, tref_reorg);
    (fco_connectblock, tref_connectblock);
    (fco_supply, tref_none) ].

(** 001M cross-check view: live-projected Coq trace vs frozen C++ (from
    [LiveRuntimeTraceIntegration]). Re-exported here for the comparison layer. *)
Definition live_cross_pairs : list (LiveTraceObservation * FrozenCppObservation) :=
  live_frozen_pairs.
