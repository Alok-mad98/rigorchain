(** Phase4FConsensus/LiveRuntimeTraceIntegration.v
    Live runtime -> Coq TraceObservation projection (PHASE-4F-001M).

    Projects imported live runtime evidence (TASK-003/004/005) into Coq-side
    [LiveTraceObservation] payloads that mirror the frozen runtime/C++ payloads,
    so the Coq-trace-vs-frozen comparison can be evaluated per component.

    *** HONESTY (critical) ***
    The resulting agreements are PROJECTION IDENTITIES: the Coq live observation
    is DEFINED as the projection of the same frozen runtime payload that the
    FrozenCpp catalog also transcribes. This is import-vs-import cross-consistency
    (a transcription/representation check), NOT an independent Coq-derived proof,
    and does NOT establish Coq = runtime or Coq = C++ (PD-4F-OB-02 / PD-4F-CPP-01
    remain OPEN/PARTIAL). It is an explicit, recorded circular dependency.

    AUDIT ONLY. No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock
    / mining code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.

(** REQUIRED-1: a Coq trace observation projected from live runtime evidence. *)
Record LiveTraceObservation := {
  lto_component : BridgeComponent;
  lto_present : bool;
  lto_payload : Z;
}.

Definition absent_live (c : BridgeComponent) : LiveTraceObservation :=
  {| lto_component := c; lto_present := false; lto_payload := 0 |}.

(** Payload selected from the runtime record: winner txid for draw, else block
    hash. *)
Definition live_payload_of (r : RuntimeEvidenceRecord) : Z :=
  match r.(rer_kind) with
  | REK_DrawWinner => r.(rer_winner_txid)
  | _ => r.(rer_block_hash)
  end.

(** REQUIRED-1: project a live runtime record into a Coq trace observation. *)
Definition project_live_runtime_to_coq_trace (r : RuntimeEvidenceRecord)
  : LiveTraceObservation :=
  {| lto_component := component_of_kind r.(rer_kind);
     lto_present := true;
     lto_payload := live_payload_of r |}.

(** REQUIRED-1: compare a projected live observation to a frozen C++ observation. *)
Definition compare_live_vs_frozen_observation
  (l : LiveTraceObservation) (o : FrozenCppObservation) : FrozenAgreementResult :=
  if negb l.(lto_present) then FAR_Missing
  else if negb o.(fco_present) then FAR_Missing
  else if Z.eqb l.(lto_payload) o.(fco_payload) then FAR_Agree else FAR_Disagree.

(** REQUIRED-1: catalog of projected live observations (Draw/Replay/Reorg/Connect). *)
Definition LiveTraceObservationCatalog : list LiveTraceObservation :=
  [ project_live_runtime_to_coq_trace ev_003_winner;     (* Draw *)
    project_live_runtime_to_coq_trace ev_005_multinode;  (* Replay *)
    project_live_runtime_to_coq_trace ev_005_reorg;      (* Reorg *)
    project_live_runtime_to_coq_trace ev_005_connect ].  (* ConnectBlock *)

(** Live-projected vs frozen C++ pairs, per component (Supply has no live obs). *)
Definition live_frozen_pairs : list (LiveTraceObservation * FrozenCppObservation) :=
  [ (project_live_runtime_to_coq_trace ev_003_winner, fco_draw);
    (project_live_runtime_to_coq_trace ev_005_multinode, fco_replay);
    (project_live_runtime_to_coq_trace ev_005_reorg, fco_reorg);
    (project_live_runtime_to_coq_trace ev_005_connect, fco_connectblock);
    (absent_live BC_Supply, fco_supply) ].

Definition live_results : list FrozenAgreementResult :=
  map (fun p => compare_live_vs_frozen_observation (fst p) (snd p)) live_frozen_pairs.

Definition live_agreement_count : nat :=
  length (filter far_is_agree live_results).
Definition live_disagreement_count : nat :=
  length (filter far_is_disagree live_results).
Definition live_missing_count : nat :=
  length (filter far_is_missing live_results).

(** ---- Projection-correctness proofs (Qed; no Admitted) ---- *)

Lemma project_present :
  forall r, (project_live_runtime_to_coq_trace r).(lto_present) = true.
Proof. intro r. reflexivity. Qed.

Lemma project_payload_eq :
  forall r, (project_live_runtime_to_coq_trace r).(lto_payload) = live_payload_of r.
Proof. intro r. reflexivity. Qed.

Lemma project_draw_payload_is_winner_txid :
  (project_live_runtime_to_coq_trace ev_003_winner).(lto_payload)
    = rer_winner_txid ev_003_winner.
Proof. reflexivity. Qed.

(** Equal payloads on two present observations project to an agreement. *)
Lemma compare_agree_of_equal :
  forall l o,
    l.(lto_present) = true ->
    o.(fco_present) = true ->
    l.(lto_payload) = o.(fco_payload) ->
    compare_live_vs_frozen_observation l o = FAR_Agree.
Proof.
  intros l o Hl Ho Hpay.
  unfold compare_live_vs_frozen_observation.
  rewrite Hl, Ho. simpl. rewrite Hpay, Z.eqb_refl. reflexivity.
Qed.

(** Honest statistics: 4 agree (projection identities), 0 disagree, 1 missing. *)
Lemma live_agreement_count_value : live_agreement_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma live_disagreement_count_value : live_disagreement_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma live_missing_count_value : live_missing_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma live_statistics_total :
  (live_agreement_count + live_disagreement_count + live_missing_count)%nat
    = length live_frozen_pairs.
Proof. vm_compute. reflexivity. Qed.
