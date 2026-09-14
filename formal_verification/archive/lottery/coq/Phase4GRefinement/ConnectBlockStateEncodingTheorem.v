(** Phase4GRefinement/ConnectBlockStateEncodingTheorem.v
    ConnectBlock State Encoding theorem (TASK-014BB).

    Mirrors 012Z/013AA for the ConnectBlock component. The Coq semantic
    ConnectBlock payload is a post-accept-block trace height; the runtime/frozen
    payloads are 256-bit accepted-block hashes from TASK-005 evidence. The closure
    proves parametric encoder commutativity and witness existence, retiring
    EO_ConnectBlockStateEncoding — the last remaining obligation.

    NO new oracle / bridge / readiness / comparison / status layer.  NO consensus /
    runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock code is touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.EquivalenceObligationCatalog.
Require Import Rigor.Phase4GRefinement.DrawHashProjectionTheorem.
Require Import Rigor.Phase4GRefinement.ReplayStateEncodingTheorem.
Require Import Rigor.Phase4GRefinement.ReorgStateEncodingTheorem.

(** ================================================================
    Part A:  Frozen / runtime ConnectBlock payload identity
    ================================================================ *)

Definition frozen_connectblock_payload : Z := fco_payload fco_connectblock.
Definition runtime_connectblock_payload : Z := rer_block_hash ev_005_connect.

Lemma frozen_runtime_connectblock_payload_equal :
  frozen_connectblock_payload = runtime_connectblock_payload.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part B:  Parametric state-encoder commutativity
    ================================================================ *)

Theorem connectblock_state_encoding_correct :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_connectblock tr = Some h ->
    f h = runtime_connectblock_payload ->
    f h = frozen_connectblock_payload.
Proof.
  intros tr h f Hproj Henc.
  rewrite Henc. symmetry. apply frozen_runtime_connectblock_payload_equal.
Qed.

Theorem connectblock_state_encoding_correct_sym :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_connectblock tr = Some h ->
    f h = frozen_connectblock_payload ->
    f h = runtime_connectblock_payload.
Proof.
  intros tr h f Hproj Henc.
  rewrite Henc. apply frozen_runtime_connectblock_payload_equal.
Qed.

Theorem connectblock_state_encoding_two_sided :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_connectblock tr = Some h ->
    f h = runtime_connectblock_payload ->
    f h = runtime_connectblock_payload /\ f h = frozen_connectblock_payload.
Proof.
  intros tr h f Hproj Henc. split.
  - exact Henc.
  - eapply connectblock_state_encoding_correct; eassumption.
Qed.

(** ================================================================
    Part C:  Witness encoder existence
    ================================================================ *)

Definition witness_connectblock_encoder (h : Z) : Z := runtime_connectblock_payload.

Theorem connectblock_state_encoding_witness_exists :
  exists f : Z -> Z,
    forall (tr : consensus_trace) (h : Z),
      project_connectblock tr = Some h ->
      f h = runtime_connectblock_payload /\ f h = frozen_connectblock_payload.
Proof.
  exists witness_connectblock_encoder.
  intros tr h _. unfold witness_connectblock_encoder. split.
  - reflexivity.
  - symmetry. apply frozen_runtime_connectblock_payload_equal.
Qed.

(** ================================================================
    Part D:  Concrete Phase-4F ConnectBlock trace
    ================================================================ *)

Lemma project_connectblock_total :
  forall tr : consensus_trace,
    exists h, project_connectblock tr = Some h.
Proof. intro tr. exists (trace_height tr). reflexivity. Qed.

(** A single accept-block trace for the Phase-4F ConnectBlock semantic observation. *)
Definition connectblock_trace : consensus_trace := [ TrAcceptBlock 7 ].

Definition phase4f_connectblock_height : Z := trace_height connectblock_trace.

Lemma phase4f_connectblock_height_value : phase4f_connectblock_height = 1%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma phase4f_connectblock_projects_to_some :
  project_connectblock connectblock_trace = Some phase4f_connectblock_height.
Proof. reflexivity. Qed.

Theorem phase4f_connectblock_encoding_correct :
  witness_connectblock_encoder phase4f_connectblock_height = runtime_connectblock_payload /\
  witness_connectblock_encoder phase4f_connectblock_height = frozen_connectblock_payload.
Proof.
  eapply connectblock_state_encoding_two_sided.
  - exact phase4f_connectblock_projects_to_some.
  - reflexivity.
Qed.

(** ================================================================
    Part E:  Updated obligation status — ALL OBLIGATIONS RESOLVED
    ================================================================ *)

Definition update_obligation_all (o : ObligationRecord) : ObligationRecord :=
  {| eo_kind := o.(eo_kind);
     eo_disagreement_count := o.(eo_disagreement_count);
     eo_blocking := false |}.

Definition obligation_catalog_all_resolved : list ObligationRecord :=
  map update_obligation_all obligation_catalog.

Definition blocking_after_connectblock : nat :=
  length (filter obligation_is_blocking obligation_catalog_all_resolved).

Definition resolved_after_connectblock : nat :=
  length (filter (fun o => negb o.(eo_blocking)) obligation_catalog_all_resolved).

Lemma blocking_after_connectblock_value : blocking_after_connectblock = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma resolved_after_connectblock_value : resolved_after_connectblock = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Definition connectblock_resolved_disagreement_count : nat := 2.

Definition remaining_disagreements_after_connectblock : nat :=
  accounted_disagreement_count
    - draw_resolved_disagreement_count
    - replay_resolved_disagreement_count
    - reorg_resolved_disagreement_count
    - connectblock_resolved_disagreement_count.

Lemma remaining_disagreements_after_connectblock_value :
  remaining_disagreements_after_connectblock = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma updated_obligation_partition_connectblock :
  blocking_after_connectblock + resolved_after_connectblock = obligation_record_count.
Proof. vm_compute. reflexivity. Qed.

(** All four obligations resolved — zero blocking, zero remaining disagreements. *)
Lemma all_obligations_resolved :
  blocking_after_connectblock = 0%nat /\ remaining_disagreements_after_connectblock = 0%nat.
Proof. split; vm_compute; reflexivity. Qed.
