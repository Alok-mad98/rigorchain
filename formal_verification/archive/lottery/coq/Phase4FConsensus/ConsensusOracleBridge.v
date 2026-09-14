(** Phase4FConsensus/ConsensusOracleBridge.v
    Oracle Bridge specification layer (PHASE-4F-001F).

    A SPECIFICATION-ONLY model of the bridge:

        Coq Trace
          -> Trace Observation
          -> Oracle Observation
          -> Agreement Result

    It does NOT connect to the runtime, does NOT execute C++, and does NOT
    import any consensus / hash / kernel module. It only types the comparison
    surface so that imported evidence can be classified honestly. AUDIT/REFINE
    only — no consensus, RFC, serialization, SHA256, ConvergeRoot, ConnectBlock,
    runtime, or golden vectors are modified. *)

From Coq Require Import List ZArith.
Import ListNotations.

(** Components the bridge reasons about. *)
Inductive BridgeComponent :=
  | BC_Draw
  | BC_Replay
  | BC_Reorg
  | BC_Supply
  | BC_ConnectBlock
  | BC_RESRuntime.

(** Where an observation comes from. Coq is the only first-class proof source;
    the others are imported / unmodeled. *)
Inductive OracleSource :=
  | OS_CoqTrace
  | OS_Runtime
  | OS_FrozenCpp
  | OS_None.

(** REQUIRED-1: an oracle-observable result value. Abstract and finite; no
    runtime semantics attached. *)
Inductive OracleResult :=
  | OR_Winner : Z -> OracleResult
  | OR_Supply : Z -> Z -> OracleResult
  | OR_Accept : bool -> OracleResult
  | OR_Root : Z -> OracleResult
  | OR_Unknown : OracleResult.

Definition eq_OracleResult (a b : OracleResult) : bool :=
  match a, b with
  | OR_Winner x, OR_Winner y => Z.eqb x y
  | OR_Supply o1 r1, OR_Supply o2 r2 => andb (Z.eqb o1 o2) (Z.eqb r1 r2)
  | OR_Accept x, OR_Accept y => Bool.eqb x y
  | OR_Root x, OR_Root y => Z.eqb x y
  | OR_Unknown, OR_Unknown => true
  | _, _ => false
  end.

Definition is_known (r : OracleResult) : bool :=
  match r with
  | OR_Unknown => false
  | _ => true
  end.

(** REQUIRED-1: observation projected from the Coq trace. *)
Record TraceObservation := {
  to_component : BridgeComponent;
  to_result : OracleResult;
}.

(** REQUIRED-1: observation reported by an external oracle (runtime / C++).
    [oo_present] is false when no oracle evidence exists for the component. *)
Record OracleObservation := {
  oo_component : BridgeComponent;
  oo_source : OracleSource;
  oo_present : bool;
  oo_result : OracleResult;
}.

(** REQUIRED-1: result of comparing a trace observation against an oracle one. *)
Inductive ObservationAgreement :=
  | OA_Agree
  | OA_Disagree
  | OA_Indeterminate.

(** REQUIRED-1: recorded bridge status (matches the matrix vocabulary). *)
Inductive BridgeStatus :=
  | BS_Match
  | BS_Partial
  | BS_Blocked
  | BS_Unknown.

(** Coq Trace -> Trace Observation -> Oracle Observation -> Agreement. *)
Definition compare_observation (t : TraceObservation) (o : OracleObservation)
  : ObservationAgreement :=
  if negb o.(oo_present) then OA_Indeterminate
  else if andb (is_known t.(to_result)) (is_known o.(oo_result)) then
    (if eq_OracleResult t.(to_result) o.(oo_result) then OA_Agree else OA_Disagree)
  else OA_Indeterminate.

(** Agreement does not by itself certify equivalence; an indeterminate /
    single-source observation maps to BLOCKED, not MATCH. *)
Definition status_of_agreement (a : ObservationAgreement) : BridgeStatus :=
  match a with
  | OA_Agree => BS_Match
  | OA_Disagree => BS_Blocked
  | OA_Indeterminate => BS_Blocked
  end.

(** A full bridge row: trace side, oracle side, and the recorded status. The
    recorded status is supplied (auditor judgement) and may be more conservative
    than the mechanical [status_of_agreement]. *)
Record BridgeRow := {
  br_component : BridgeComponent;
  br_trace : TraceObservation;
  br_oracle : OracleObservation;
  br_recorded : BridgeStatus;
}.

Definition row_agreement (r : BridgeRow) : ObservationAgreement :=
  compare_observation r.(br_trace) r.(br_oracle).

(** ---- Soundness shape lemmas (Qed; no equivalence asserted) ---- *)

(** A single-source observation (no oracle present) can never be MATCH. *)
Lemma absent_oracle_not_match :
  forall t o,
    o.(oo_present) = false ->
    status_of_agreement (compare_observation t o) = BS_Blocked.
Proof.
  intros t o Habs. unfold compare_observation. rewrite Habs. reflexivity.
Qed.

(** MATCH requires genuine two-sided agreement on known results. *)
Lemma match_requires_agreement :
  forall t o,
    status_of_agreement (compare_observation t o) = BS_Match ->
    compare_observation t o = OA_Agree.
Proof.
  intros t o H.
  destruct (compare_observation t o) eqn:E; simpl in H; try discriminate; reflexivity.
Qed.

Lemma agree_implies_eq_result :
  forall t o,
    compare_observation t o = OA_Agree ->
    eq_OracleResult t.(to_result) o.(oo_result) = true.
Proof.
  intros t o H. unfold compare_observation in H.
  destruct (oo_present o) eqn:Hp; simpl in H; [| discriminate].
  destruct (andb (is_known (to_result t)) (is_known (oo_result o))) eqn:Hk;
    [| discriminate].
  destruct (eq_OracleResult (to_result t) (oo_result o)) eqn:He;
    [reflexivity | discriminate].
Qed.

(** ----------------------------------------------------------------------
    PD-4F-OB-02 (OPEN): a proven Coq-trace <-> frozen-C++ equivalence theorem is
    NOT available (no C++ oracle is wired).

    The obligation is stated as a [Prop] *definition*: nothing here asserts it. *)
Definition bridge_cpp_equivalence_obligation : Prop :=
  forall r : BridgeRow, row_agreement r = OA_Agree.

(** Historical name retained for doc cross-references. It previously read
    [Theorem bridge_cpp_equivalence_placeholder : True. Admitted.] — a vacuous
    statement that could be miscited as evidence of C++ equivalence. It now
    states the true safety fact that makes the gap explicit: with no oracle
    present the bridge can never report [BS_Match], so
    [bridge_cpp_equivalence_obligation] cannot be discharged from current
    evidence. *)
Theorem bridge_cpp_equivalence_placeholder :
  forall r : BridgeRow,
    (br_oracle r).(oo_present) = false ->
    status_of_agreement (row_agreement r) = BS_Blocked.
Proof.
  intros r Habs. unfold row_agreement.
  apply absent_oracle_not_match. exact Habs.
Qed.
