(** Phase4FConsensus/SemanticVsLiveRuntimeComparison.v
    Coq-semantic vs Live-runtime comparison layer (PHASE-4F-003P).

    Compares the INDEPENDENTLY DERIVED Coq semantic observations (001N,
    SemanticObservationDerivation) against the live runtime observations
    (001M LiveTraceObservation, projected from TASK-003/004/005 evidence) per
    component. Reuses the 001K 3-valued result.

    *** No Frozen C++ observation is used as comparison content. Semantic payloads
    come from the Coq model (heights / winner index); live payloads are imported
    runtime hashes/txids. Their payloads live in different domains, so disagreement
    is EXPECTED and is NOT a defect — it documents that the abstract Coq model
    values are not the live runtime hash values. *** AUDIT ONLY: MUST NOT claim
    equivalence, Coq = Runtime, or Coq = C++. No consensus / runtime / RFC /
    SHA256 / ConvergeRoot / ConnectBlock / mining / RPC / wallet code modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.

(** Reuse the 001K 3-valued result (FAR_Agree | FAR_Disagree | FAR_Missing). *)
Definition SemanticRuntimeAgreementResult := FrozenAgreementResult.

(** REQUIRED-1: compare one Coq semantic observation to a live runtime observation.
    Missing if either side is absent; equal payload => Agree, else Disagree.
    Agreement here is raw payload equality, NOT semantic equivalence. *)
Definition compare_semantic_vs_live_runtime_observation
  (s : SemanticObservation) (l : LiveTraceObservation) : SemanticRuntimeAgreementResult :=
  if negb s.(so_present) then FAR_Missing
  else if negb l.(lto_present) then FAR_Missing
  else if Z.eqb s.(so_payload) l.(lto_payload) then FAR_Agree else FAR_Disagree.

(** Per-component comparison helpers (Draw / Replay / Reorg / ConnectBlock). *)
Definition compare_draw_sr (s : SemanticObservation) (l : LiveTraceObservation) :=
  compare_semantic_vs_live_runtime_observation s l.
Definition compare_replay_sr (s : SemanticObservation) (l : LiveTraceObservation) :=
  compare_semantic_vs_live_runtime_observation s l.
Definition compare_reorg_sr (s : SemanticObservation) (l : LiveTraceObservation) :=
  compare_semantic_vs_live_runtime_observation s l.
Definition compare_connectblock_sr (s : SemanticObservation) (l : LiveTraceObservation) :=
  compare_semantic_vs_live_runtime_observation s l.

Definition semantic_runtime_results
  (ps : list (SemanticObservation * LiveTraceObservation)) : list SemanticRuntimeAgreementResult :=
  map (fun p => compare_semantic_vs_live_runtime_observation (fst p) (snd p)) ps.

(** REQUIRED-1: counts over all (semantic, live runtime) pairs. *)
Definition semantic_trace_vs_runtime_agreement_count
  (ps : list (SemanticObservation * LiveTraceObservation)) : nat :=
  length (filter far_is_agree (semantic_runtime_results ps)).
Definition semantic_trace_vs_runtime_disagreement_count
  (ps : list (SemanticObservation * LiveTraceObservation)) : nat :=
  length (filter far_is_disagree (semantic_runtime_results ps)).
Definition semantic_trace_vs_runtime_missing_count
  (ps : list (SemanticObservation * LiveTraceObservation)) : nat :=
  length (filter far_is_missing (semantic_runtime_results ps)).

(** REQUIRED-1: comparison soundness — when both sides are present, the result is
    exactly payload equality (Agree) or inequality (Disagree). *)
Lemma compare_semantic_vs_live_runtime_observation_sound :
  forall s l,
    s.(so_present) = true ->
    l.(lto_present) = true ->
    compare_semantic_vs_live_runtime_observation s l =
      (if Z.eqb s.(so_payload) l.(lto_payload) then FAR_Agree else FAR_Disagree).
Proof.
  intros s l Hs Hl.
  unfold compare_semantic_vs_live_runtime_observation.
  rewrite Hs, Hl. simpl. reflexivity.
Qed.

(** Missing exactly characterises an absent side. *)
Lemma semantic_vs_runtime_missing_iff_absent :
  forall s l,
    compare_semantic_vs_live_runtime_observation s l = FAR_Missing <->
    (s.(so_present) = false \/ l.(lto_present) = false).
Proof.
  intros s l. unfold compare_semantic_vs_live_runtime_observation.
  destruct (so_present s) eqn:Hs; destruct (lto_present l) eqn:Hl; simpl.
  - destruct (Z.eqb (so_payload s) (lto_payload l));
      split; intros H; try discriminate; destruct H; discriminate.
  - split; intros _; [ right; reflexivity | reflexivity ].
  - split; intros _; [ left; reflexivity | reflexivity ].
  - split; intros _; [ left; reflexivity | reflexivity ].
Qed.
