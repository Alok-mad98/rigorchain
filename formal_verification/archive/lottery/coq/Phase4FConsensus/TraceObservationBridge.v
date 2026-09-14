(** Phase4FConsensus/TraceObservationBridge.v
    Coq-trace -> Oracle observation bridge (PHASE-4F-001H).

    Produces the COQ SIDE of the Oracle Bridge: observations whose source is
    [OS_CoqTrace], derived from Phase-4F trace execution artifacts ONLY. No
    imported runtime evidence is used here. AUDIT / REFINE only: no consensus /
    C++ / RFC / SHA256 / ConvergeRoot / ConnectBlock / mining / runtime code is
    touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.

(** REQUIRED-1: kinds of Coq-trace observation. *)
Inductive TraceObservationKind :=
  | TOK_DrawWinner
  | TOK_RegistryExecution
  | TOK_Projection.

(** REQUIRED-1: a Coq-trace observation record, built from trace artifacts. *)
Record TraceObservationRecord := {
  tor_kind : TraceObservationKind;
  tor_component : BridgeComponent;
  tor_result : OracleResult;
}.

(** REQUIRED-1: Coq trace record -> OracleObservation.
    Source is fixed to [OS_CoqTrace]; observations are always present. *)
Definition trace_to_observation (r : TraceObservationRecord) : OracleObservation :=
  {| oo_component := r.(tor_component);
     oo_source := OS_CoqTrace;
     oo_present := true;
     oo_result := r.(tor_result) |}.

(** The trace side as fed to [compare_observation]. *)
Definition record_trace_side (r : TraceObservationRecord) : TraceObservation :=
  {| to_component := r.(tor_component);
     to_result := r.(tor_result) |}.

Lemma trace_observation_present :
  forall r, (trace_to_observation r).(oo_present) = true.
Proof. intro r. reflexivity. Qed.

Lemma trace_observation_coq_source :
  forall r, (trace_to_observation r).(oo_source) = OS_CoqTrace.
Proof. intro r. reflexivity. Qed.
