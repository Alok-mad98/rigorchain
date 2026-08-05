(** Phase4FConsensus/FrozenCppAgreementCatalog.v
    Mapping of frozen C++ observations to closest references (PHASE-4F-001K).

    Each frozen C++ observation is paired with the CLOSEST already-available
    reference: a Coq-trace observation where one exists with a comparable txid
    (Draw), otherwise a runtime observation (ConnectBlock/Reorg/Replay), or none
    (Supply). No new observations, no external inputs, no JSON, no runtime calls. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationAudit.

(** Coq-trace draw reference: the Coq-side winner txid (golden domain), taken
    from the draw representation catalog. This is NOT the live C++ block winner,
    so the Draw comparison is expected to DISAGREE — the honest signal that the
    Coq trace domain is not identified with the frozen C++ output. *)
Definition coq_draw_ref_txid : Z :=
  match representation_catalog with
  | e :: _ => entry_coq_txid e
  | [] => 0
  end.

Definition ref_coq_draw : FrozenReference :=
  {| fr_present := true; fr_payload := coq_draw_ref_txid; fr_source := 1 |}.

(** Runtime references (same frozen evidence, independent import path). *)
Definition ref_runtime_connect : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_connect; fr_source := 2 |}.

Definition ref_runtime_reorg : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_reorg; fr_source := 2 |}.

Definition ref_runtime_replay : FrozenReference :=
  {| fr_present := true; fr_payload := rer_block_hash ev_005_multinode; fr_source := 2 |}.

(** Supply: no comparable reference available. *)
Definition ref_none : FrozenReference :=
  {| fr_present := false; fr_payload := 0; fr_source := 0 |}.

(** REQUIRED-2: the frozen C++ -> reference mapping. *)
Definition frozen_agreement_pairs : list (FrozenCppObservation * FrozenReference) :=
  [ (fco_draw, ref_coq_draw);
    (fco_connectblock, ref_runtime_connect);
    (fco_reorg, ref_runtime_reorg);
    (fco_replay, ref_runtime_replay);
    (fco_supply, ref_none) ].
