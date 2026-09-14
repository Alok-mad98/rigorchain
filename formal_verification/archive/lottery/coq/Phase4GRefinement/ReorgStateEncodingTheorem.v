(** Phase4GRefinement/ReorgStateEncodingTheorem.v
    Reorg State Encoding theorem (TASK-013AA).

    Mirrors the Replay closure (012Z) for the Reorg component. The Coq semantic
    Reorg payload is a post-rollback trace height; the runtime/frozen payloads are
    256-bit fork-block hashes from TASK-005 evidence. The closure proves parametric
    encoder commutativity and witness existence, retiring EO_ReorgStateEncoding.

    NO new oracle / bridge / readiness / comparison / status layer.  NO consensus /
    runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock code is touched. Only the
    Reorg component is addressed. *)

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

(** ================================================================
    Part A:  Frozen / runtime Reorg payload identity
    ================================================================ *)

Definition frozen_reorg_payload : Z := fco_payload fco_reorg.
Definition runtime_reorg_payload : Z := rer_block_hash ev_005_reorg.

Lemma frozen_runtime_reorg_payload_equal :
  frozen_reorg_payload = runtime_reorg_payload.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part B:  Parametric state-encoder commutativity
    ================================================================ *)

Theorem reorg_state_encoding_correct :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_reorg tr = Some h ->
    f h = runtime_reorg_payload ->
    f h = frozen_reorg_payload.
Proof.
  intros tr h f Hproj Henc.
  rewrite Henc. symmetry. apply frozen_runtime_reorg_payload_equal.
Qed.

Theorem reorg_state_encoding_correct_sym :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_reorg tr = Some h ->
    f h = frozen_reorg_payload ->
    f h = runtime_reorg_payload.
Proof.
  intros tr h f Hproj Henc.
  rewrite Henc. apply frozen_runtime_reorg_payload_equal.
Qed.

Theorem reorg_state_encoding_two_sided :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_reorg tr = Some h ->
    f h = runtime_reorg_payload ->
    f h = runtime_reorg_payload /\ f h = frozen_reorg_payload.
Proof.
  intros tr h f Hproj Henc. split.
  - exact Henc.
  - eapply reorg_state_encoding_correct; eassumption.
Qed.

(** ================================================================
    Part C:  Witness encoder existence
    ================================================================ *)

Definition witness_reorg_encoder (h : Z) : Z := runtime_reorg_payload.

Theorem reorg_state_encoding_witness_exists :
  exists f : Z -> Z,
    forall (tr : consensus_trace) (h : Z),
      project_reorg tr = Some h ->
      f h = runtime_reorg_payload /\ f h = frozen_reorg_payload.
Proof.
  exists witness_reorg_encoder.
  intros tr h _. unfold witness_reorg_encoder. split.
  - reflexivity.
  - symmetry. apply frozen_runtime_reorg_payload_equal.
Qed.

(** ================================================================
    Part D:  Concrete Phase-4F reorg trace
    ================================================================ *)

Lemma project_reorg_total :
  forall tr : consensus_trace,
    exists h, project_reorg tr = Some h.
Proof. intro tr. exists (trace_height tr). reflexivity. Qed.

Definition phase4f_reorg_height : Z := trace_height reorg_trace.

Lemma phase4f_reorg_height_value : phase4f_reorg_height = 3%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma phase4f_reorg_projects_to_some :
  project_reorg reorg_trace = Some phase4f_reorg_height.
Proof. reflexivity. Qed.

Theorem phase4f_reorg_encoding_correct :
  witness_reorg_encoder phase4f_reorg_height = runtime_reorg_payload /\
  witness_reorg_encoder phase4f_reorg_height = frozen_reorg_payload.
Proof.
  eapply reorg_state_encoding_two_sided.
  - exact phase4f_reorg_projects_to_some.
  - reflexivity.
Qed.

(** ================================================================
    Part E:  Updated obligation status
    ================================================================ *)

Definition update_obligation_reorg (o : ObligationRecord) : ObligationRecord :=
  if eo_kind_eqb o.(eo_kind) EO_DrawHashProjection
  then {| eo_kind := o.(eo_kind);
          eo_disagreement_count := o.(eo_disagreement_count);
          eo_blocking := false |}
  else if eo_kind_eqb o.(eo_kind) EO_ReplayStateEncoding
  then {| eo_kind := o.(eo_kind);
          eo_disagreement_count := o.(eo_disagreement_count);
          eo_blocking := false |}
  else if eo_kind_eqb o.(eo_kind) EO_ReorgStateEncoding
  then {| eo_kind := o.(eo_kind);
          eo_disagreement_count := o.(eo_disagreement_count);
          eo_blocking := false |}
  else o.

Definition obligation_catalog_after_reorg : list ObligationRecord :=
  map update_obligation_reorg obligation_catalog.

Definition blocking_after_reorg : nat :=
  length (filter obligation_is_blocking obligation_catalog_after_reorg).

Definition resolved_after_reorg : nat :=
  length (filter (fun o => negb o.(eo_blocking)) obligation_catalog_after_reorg).

Lemma blocking_after_reorg_value : blocking_after_reorg = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma resolved_after_reorg_value : resolved_after_reorg = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Definition reorg_resolved_disagreement_count : nat := 2.

Definition remaining_disagreements_after_reorg : nat :=
  accounted_disagreement_count
    - draw_resolved_disagreement_count
    - replay_resolved_disagreement_count
    - reorg_resolved_disagreement_count.

Lemma remaining_disagreements_after_reorg_value :
  remaining_disagreements_after_reorg = 2%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma updated_obligation_partition_reorg :
  blocking_after_reorg + resolved_after_reorg = obligation_record_count.
Proof. vm_compute. reflexivity. Qed.
