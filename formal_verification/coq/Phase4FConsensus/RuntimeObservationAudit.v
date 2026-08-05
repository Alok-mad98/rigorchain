(** Phase4FConsensus/RuntimeObservationAudit.v
    Runtime observation presence audit (PHASE-4F-001G).

    Confirms that every catalogued frozen evidence record yields a PRESENT oracle
    observation. PRESENT means "a runtime observation exists" — it does NOT mean
    the observation has been shown equivalent to the Coq trace. AUDIT ONLY. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.

(** REQUIRED-3: presence status of a runtime observation. *)
Inductive RuntimeObservationStatus :=
  | ROS_Present
  | ROS_Missing.

Definition runtime_observation_status (o : OracleObservation) : RuntimeObservationStatus :=
  if o.(oo_present) then ROS_Present else ROS_Missing.

(** REQUIRED-3: present and missing are distinct. *)
Lemma present_observation_not_missing :
  ROS_Present <> ROS_Missing.
Proof. discriminate. Qed.

(** Observation status for a runtime evidence record. *)
Definition record_observation_status (r : RuntimeEvidenceRecord) : RuntimeObservationStatus :=
  runtime_observation_status (evidence_to_observation r).

Definition catalog_observations : list OracleObservation :=
  map evidence_to_observation runtime_evidence_catalog.

Definition catalog_observation_statuses : list RuntimeObservationStatus :=
  map record_observation_status runtime_evidence_catalog.

(** A single record always produces a PRESENT observation. *)
Lemma record_observation_present :
  forall r, record_observation_status r = ROS_Present.
Proof.
  intro r. unfold record_observation_status, runtime_observation_status.
  rewrite evidence_observation_present. reflexivity.
Qed.

(** REQUIRED-3: every catalog entry produces a PRESENT observation. *)
Lemma catalog_entries_produce_present_observations :
  forall r, In r runtime_evidence_catalog ->
            record_observation_status r = ROS_Present.
Proof.
  intros r _. apply record_observation_present.
Qed.

(** Corollary: no catalog observation is missing. *)
Lemma catalog_no_missing_observations :
  forall s, In s catalog_observation_statuses -> s = ROS_Present.
Proof.
  intros s Hin. unfold catalog_observation_statuses in Hin.
  apply in_map_iff in Hin. destruct Hin as [r [Heq _]].
  rewrite <- Heq. apply record_observation_present.
Qed.

Definition catalog_present_count : nat :=
  length (filter (fun s => match s with ROS_Present => true | _ => false end)
                 catalog_observation_statuses).

Lemma catalog_present_count_value : catalog_present_count = 6%nat.
Proof. reflexivity. Qed.
