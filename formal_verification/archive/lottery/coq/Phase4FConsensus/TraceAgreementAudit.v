(** Phase4FConsensus/TraceAgreementAudit.v
    Trace vs runtime observation agreement audit (PHASE-4F-001H).

    Pairs each imported runtime observation (TASK-003/004/005, frozen) with the
    corresponding Coq-trace observation and runs the EXISTING
    [compare_observation] from the Oracle Bridge. This advances Phase-4F from
    "runtime evidence present" to "trace-agreement candidate". It does NOT
    establish Coq = Runtime: the only directly comparable payload (draw winner)
    is an index on the Coq side and a txid on the runtime side, so it DISAGREES;
    all other runtime observations have no comparable Coq payload (MISSING).
    AUDIT only — no consensus / runtime / RFC / evidence modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.TraceObservationBridge.
Require Import Rigor.Phase4FConsensus.TraceObservationCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.

(** Missing == the bridge's "indeterminate" agreement (no comparable payload). *)
Definition OA_Missing : ObservationAgreement := OA_Indeterminate.

(** Coq-trace side for the draw winner pairing (index domain). *)
Definition draw_trace_side : TraceObservation :=
  {| to_component := BC_Draw; to_result := OR_Winner draw_trace_winner |}.

(** An "unknown" Coq-trace side for components Phase-4F does not yet model. *)
Definition unknown_trace_side (c : BridgeComponent) : TraceObservation :=
  {| to_component := c; to_result := OR_Unknown |}.

(** Pairs: (Coq-trace observation, frozen runtime observation), aligned to the
    runtime catalog (TASK-003/004/005). *)
Definition trace_runtime_pairs : list (TraceObservation * OracleObservation) :=
  [ (draw_trace_side, evidence_to_observation ev_003_winner);
    (unknown_trace_side BC_RESRuntime, evidence_to_observation ev_003_mint);
    (unknown_trace_side BC_RESRuntime, evidence_to_observation ev_004_soak);
    (unknown_trace_side BC_ConnectBlock, evidence_to_observation ev_005_connect);
    (unknown_trace_side BC_Reorg, evidence_to_observation ev_005_reorg);
    (unknown_trace_side BC_Replay, evidence_to_observation ev_005_multinode) ].

(** REQUIRED-3: agreement result / status per pair via the existing bridge. *)
Definition agreement_result (p : TraceObservation * OracleObservation)
  : ObservationAgreement :=
  compare_observation (fst p) (snd p).

Definition agreement_status (p : TraceObservation * OracleObservation)
  : ObservationAgreement :=
  agreement_result p.

Definition pair_results : list ObservationAgreement :=
  map agreement_result trace_runtime_pairs.

Definition is_agree (a : ObservationAgreement) : bool :=
  match a with OA_Agree => true | _ => false end.
Definition is_disagree (a : ObservationAgreement) : bool :=
  match a with OA_Disagree => true | _ => false end.
Definition is_missing (a : ObservationAgreement) : bool :=
  match a with OA_Indeterminate => true | _ => false end.

Definition agreement_count : nat := length (filter is_agree pair_results).
Definition disagreement_count : nat := length (filter is_disagree pair_results).
Definition missing_count : nat := length (filter is_missing pair_results).

(** Honest counts: 0 agree, 1 disagree (draw index vs txid), 5 missing. *)
Lemma agreement_count_value : agreement_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma disagreement_count_value : disagreement_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma missing_count_value : missing_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma counts_total :
  (agreement_count + disagreement_count + missing_count)%nat = length trace_runtime_pairs.
Proof. vm_compute. reflexivity. Qed.

(** ---- REQUIRED-4: mandatory Qed proofs (no Admitted) ---- *)

(** Agreement implies the two observed payloads are equal. *)
Theorem agree_implies_same_payload :
  forall t o,
    compare_observation t o = OA_Agree ->
    eq_OracleResult (to_result t) (oo_result o) = true.
Proof.
  intros t o H. apply agree_implies_eq_result. exact H.
Qed.

(** A missing (indeterminate) result is never an agreement. *)
Theorem missing_not_agree : OA_Missing <> OA_Agree.
Proof. unfold OA_Missing. discriminate. Qed.

(** A disagreement never maps to a MATCH bridge status. *)
Theorem disagree_not_match :
  status_of_agreement OA_Disagree <> BS_Match.
Proof. simpl. discriminate. Qed.
