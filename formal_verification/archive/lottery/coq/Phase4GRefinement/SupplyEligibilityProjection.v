(** Phase4GRefinement/SupplyEligibilityProjection.v
    Supply + Eligibility/sigma Coq-side projection (TASK-014CC).

    SPEC ONLY / AUDIT ONLY. Projects two previously-Missing/Imported components
    into Coq semantic observations, in a way that is identity-faithful to the
    Phase-4F sources (no ad-hoc payload fabrication, no consensus / runtime / RFC
    / SHA256 / ConvergeRoot / ConnectBlock / Replay / Reorg code modified).

    Two honesty boundaries:

    * SUPPLY: the Coq [ConsensusState] carries [cs_supply_oru] / [cs_supply_res],
      both initialised to 0 and unchanged by [TrAcceptBlock] / [TrRollback].
      The frozen Supply observation is absent (payload 0); the runtime side has no
      single supply payload. The projection records the Coq-state supply total
      ([= 0] for any trace from [init_consensus_state]). This yields a structural
      [0 = 0] identity with the absent frozen payload — useful as a typed projection
      but NOT a real supply-equivalence claim. Status: PS_Partial.

    * ELIGIBILITY / SIGMA: the [dc_eligible] flags are IMPORTED kernel evidence
      (PD-4F-OB-03). The projection types these into a Coq-side boolean list
      [map dc_eligible cands]; this is faithful to the import (identity), NOT a
      Coq re-derivation of eligibility (which would require the hash kernel and
      sigma threshold, out of scope). Status: PS_Partial.

    *** Coq = Runtime / Coq = Frozen C++ equivalence is NOT claimed. *** *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4GRefinement.RefinementProjection.

(** ================================================================
    Part A:  Supply projection from Coq machine state
    ================================================================ *)

(** Total Coq-state supply: ORU + RES (both initialised to 0). *)
Definition project_supply_payload (s : ConsensusState) : Z :=
  s.(cs_supply_oru) + s.(cs_supply_res).

(** Project the supply total reached after replaying a trace. *)
Definition project_supply_after_trace (tr : consensus_trace) : Z :=
  match execute_trace init_consensus_state tr with
  | Some s => project_supply_payload s
  | None => 0
  end.

(** Initial state has zero supply. *)
Lemma init_supply_zero :
  project_supply_payload init_consensus_state = 0%Z.
Proof. reflexivity. Qed.

(** Supply preservation across any single transition. *)
Lemma transition_preserves_supply :
  forall s t s',
    execute_transition s t = Some s' ->
    project_supply_payload s' = project_supply_payload s.
Proof.
  intros s t s' Heq. destruct t.
  - simpl in Heq. injection Heq as <-. reflexivity.
  - simpl in Heq. destruct (Z.ltb (Z.of_nat n) s.(cs_height)).
    + injection Heq as <-. reflexivity.
    + discriminate.
  - simpl in Heq. injection Heq as <-. reflexivity.
Qed.

(** Supply preservation across an entire trace (Some/None case). *)
Lemma execute_trace_preserves_supply :
  forall tr s,
    match execute_trace s tr with
    | Some s' => project_supply_payload s' = project_supply_payload s
    | None => True
    end.
Proof.
  induction tr as [|t rest IH]; intros s.
  - simpl. reflexivity.
  - simpl. destruct (execute_transition s t) eqn:Heq.
    + specialize (IH c). destruct (execute_trace c rest) eqn:Heq2.
      * rewrite IH. eapply transition_preserves_supply; eauto.
      * exact I.
    + exact I.
Qed.

(** Soundness: any trace from [init_consensus_state] yields zero supply,
    matching the frozen Supply payload (which is also 0, absent). *)
Lemma project_supply_after_trace_zero :
  forall tr, project_supply_after_trace tr = 0%Z.
Proof.
  intro tr. unfold project_supply_after_trace.
  pose proof (execute_trace_preserves_supply tr init_consensus_state) as H.
  destruct (execute_trace init_consensus_state tr) as [s'|] eqn:Heq.
  - rewrite H. apply init_supply_zero.
  - reflexivity.
Qed.

(** Frozen Supply payload (absent, 0). *)
Definition frozen_supply_payload : Z := fco_payload fco_supply.

Lemma frozen_supply_is_zero : frozen_supply_payload = 0%Z.
Proof. reflexivity. Qed.

(** Structural identity: Coq-projected supply = frozen supply payload (both 0).
    This is a typed-projection identity, NOT a real supply-equivalence claim. *)
Theorem supply_projection_structural_identity :
  forall tr, project_supply_after_trace tr = frozen_supply_payload.
Proof.
  intro tr. rewrite project_supply_after_trace_zero, frozen_supply_is_zero.
  reflexivity.
Qed.

(** ================================================================
    Part B:  Eligibility / sigma projection from imported candidates
    ================================================================ *)

(** Project the imported eligibility flags into a Coq-side boolean list. *)
Definition project_eligibility_sigma (cands : list DrawCandidate) : list bool :=
  map dc_eligible cands.

(** Count of eligible candidates in the projected list. *)
Definition project_eligibility_count (cands : list DrawCandidate) : nat :=
  length (filter (fun b : bool => b) (project_eligibility_sigma cands)).

(** Identity faithfulness: the projection preserves length. *)
Lemma project_eligibility_sigma_length :
  forall cands, length (project_eligibility_sigma cands) = length cands.
Proof. intro cands. apply map_length. Qed.

(** Identity faithfulness: each projected boolean equals the imported flag. *)
Lemma project_eligibility_sigma_nth :
  forall cands i,
    nth_error (project_eligibility_sigma cands) i
      = option_map dc_eligible (nth_error cands i).
Proof.
  induction cands as [|c rest IH]; intros [|i].
  - reflexivity.
  - reflexivity.
  - reflexivity.
  - simpl. apply IH.
Qed.

(** Soundness: the projection is exactly [map dc_eligible] (no fabrication). *)
Theorem eligibility_projection_is_identity_map :
  forall cands, project_eligibility_sigma cands = map dc_eligible cands.
Proof. intro cands. reflexivity. Qed.

(** ================================================================
    Part C:  Projection status for Supply and Eligibility
    ================================================================ *)

(** Supply / Eligibility projection status — both PS_Partial.
    Supply is structurally identical to the frozen absent payload (0 = 0) but
    the Coq machine does not yet model real minting.
    Eligibility is identity over imported flags (no Coq re-derivation). *)
Definition supply_projection_status : ProjectionStatus := PS_Partial.
Definition eligibility_projection_status : ProjectionStatus := PS_Partial.

Lemma supply_projection_status_value :
  supply_projection_status = PS_Partial.
Proof. reflexivity. Qed.

Lemma eligibility_projection_status_value :
  eligibility_projection_status = PS_Partial.
Proof. reflexivity. Qed.
