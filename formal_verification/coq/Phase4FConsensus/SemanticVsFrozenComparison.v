(** Phase4FConsensus/SemanticVsFrozenComparison.v
    Coq-semantic vs Frozen-C++ comparison layer (PHASE-4F-002O).

    Compares the INDEPENDENTLY DERIVED Coq semantic observations (001N,
    SemanticObservationDerivation) against the frozen C++ Oracle observations
    (001J, FrozenCppOracle) per component. Reuses the 001K 3-valued result.

    *** No runtime projection is used. Semantic payloads come from the Coq model
    (heights / winner index); frozen payloads are imported frozen-C++ hashes.
    Their payloads live in different domains, so disagreement is EXPECTED and is
    NOT a defect — it documents that the abstract Coq model values are not the
    frozen C++ hash values. *** AUDIT ONLY: MUST NOT claim equivalence,
    Coq = C++, or Coq = Frozen C++. No consensus / runtime / RFC / SHA256 /
    ConvergeRoot / ConnectBlock / mining / RPC / wallet code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.

(** Reuse the 001K 3-valued result (FAR_Agree | FAR_Disagree | FAR_Missing). *)
Definition SemanticFrozenAgreementResult := FrozenAgreementResult.

(** REQUIRED-1: compare one Coq semantic observation to a frozen C++ observation.
    Missing if either side is absent; equal payload => Agree, else Disagree.
    Agreement here is raw payload equality, NOT semantic equivalence. *)
Definition compare_semantic_vs_frozen_observation
  (s : SemanticObservation) (o : FrozenCppObservation) : SemanticFrozenAgreementResult :=
  if negb s.(so_present) then FAR_Missing
  else if negb o.(fco_present) then FAR_Missing
  else if Z.eqb s.(so_payload) o.(fco_payload) then FAR_Agree else FAR_Disagree.

(** Per-component comparison helpers (Draw / Replay / Reorg / ConnectBlock). *)
Definition compare_draw_sf (s : SemanticObservation) (o : FrozenCppObservation) :=
  compare_semantic_vs_frozen_observation s o.
Definition compare_replay_sf (s : SemanticObservation) (o : FrozenCppObservation) :=
  compare_semantic_vs_frozen_observation s o.
Definition compare_reorg_sf (s : SemanticObservation) (o : FrozenCppObservation) :=
  compare_semantic_vs_frozen_observation s o.
Definition compare_connectblock_sf (s : SemanticObservation) (o : FrozenCppObservation) :=
  compare_semantic_vs_frozen_observation s o.

Definition semantic_frozen_results
  (ps : list (SemanticObservation * FrozenCppObservation)) : list SemanticFrozenAgreementResult :=
  map (fun p => compare_semantic_vs_frozen_observation (fst p) (snd p)) ps.

(** REQUIRED-1: counts over all (semantic, frozen) pairs. *)
Definition semantic_trace_agreement_count
  (ps : list (SemanticObservation * FrozenCppObservation)) : nat :=
  length (filter far_is_agree (semantic_frozen_results ps)).
Definition semantic_trace_disagreement_count
  (ps : list (SemanticObservation * FrozenCppObservation)) : nat :=
  length (filter far_is_disagree (semantic_frozen_results ps)).
Definition semantic_trace_missing_count
  (ps : list (SemanticObservation * FrozenCppObservation)) : nat :=
  length (filter far_is_missing (semantic_frozen_results ps)).

(** REQUIRED-1: comparison soundness — when both sides are present, the result is
    exactly payload equality (Agree) or inequality (Disagree). *)
Lemma compare_semantic_vs_frozen_observation_sound :
  forall s o,
    s.(so_present) = true ->
    o.(fco_present) = true ->
    compare_semantic_vs_frozen_observation s o =
      (if Z.eqb s.(so_payload) o.(fco_payload) then FAR_Agree else FAR_Disagree).
Proof.
  intros s o Hs Ho.
  unfold compare_semantic_vs_frozen_observation.
  rewrite Hs, Ho. simpl. reflexivity.
Qed.

(** Missing exactly characterises an absent side. *)
Lemma semantic_vs_frozen_missing_iff_absent :
  forall s o,
    compare_semantic_vs_frozen_observation s o = FAR_Missing <->
    (s.(so_present) = false \/ o.(fco_present) = false).
Proof.
  intros s o. unfold compare_semantic_vs_frozen_observation.
  destruct (so_present s) eqn:Hs; destruct (fco_present o) eqn:Ho; simpl.
  - destruct (Z.eqb (so_payload s) (fco_payload o));
      split; intros H; try discriminate; destruct H; discriminate.
  - split; intros _; [ right; reflexivity | reflexivity ].
  - split; intros _; [ left; reflexivity | reflexivity ].
  - split; intros _; [ left; reflexivity | reflexivity ].
Qed.
