(** Phase5Equivalence/ReplayEquivalence.v
    Replay equivalence proofs (TASK-018FF-C).

    Records the Replay equivalence projection chain:

      Coq replay trace  --[execute_trace + height_of]-->  coq_replay_height (Z)
                                                                |
                            (height vs 256-bit hash domain mismatch)
                                                                v
      frozen replay payload (TASK-005 multinode tip hash)  =  runtime replay payload

    HONESTY BOUNDARY:
    - Frozen-vs-runtime payload identity is discharged (re-export of 012Z).
    - Coq projection [project_replay] is total: every trace projects to Some height.
    - Coq semantic payload (height) does NOT match the 256-bit frozen/runtime
      payload — recorded as DISAGREEMENT, not silently bridged.
    - The encoder used here ([trace_height]) is a placeholder identity-style
      projection (height domain only); marks Replay PARTIAL.
    - Draw, Reorg, ConnectBlock, Supply are untouched.
    - No fabricated equivalence; no consensus / runtime / RFC / SHA-256 /
      ConvergeRoot / ConnectBlock / mining / wallet code modified. *)

From Coq Require Import List ZArith Bool.
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
Require Import Rigor.Phase4GRefinement.ReplayStateEncodingTheorem.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** ================================================================
    Part A:  Replay equivalence result vocabulary
    ================================================================ *)

Inductive ReplayEquivKind :=
| REK_FrozenRuntimeIdentity
| REK_ProjectionTotality
| REK_SemanticVsFrozen
| REK_SemanticVsRuntime.

Inductive ReplayEquivResult :=
| RER_Resolved
| RER_Disagreement
| RER_Missing
| RER_Partial.

Definition replay_result_resolved (r : ReplayEquivResult) : bool :=
  match r with
  | RER_Resolved => true
  | _ => false
  end.

Definition replay_result_disagreement (r : ReplayEquivResult) : bool :=
  match r with
  | RER_Disagreement => true
  | _ => false
  end.

Definition replay_result_missing (r : ReplayEquivResult) : bool :=
  match r with
  | RER_Missing => true
  | _ => false
  end.

Definition replay_result_partial (r : ReplayEquivResult) : bool :=
  match r with
  | RER_Partial => true
  | _ => false
  end.

(** [project_replay] is [Some (trace_height tr)] — definitionally an identity
    wrapper over the height field. Marks the projection layer PARTIAL. *)
Definition replay_projection_uses_identity_map : bool := true.

Lemma project_replay_is_some_trace_height :
  forall tr, project_replay tr = Some (trace_height tr).
Proof. intros tr. reflexivity. Qed.

(** ================================================================
    Part B:  Frozen <-> Runtime payload identity (TASK-005 multinode tip)
    ================================================================ *)

Definition replay_frozen_payload : Z := frozen_replay_payload.
Definition replay_runtime_payload : Z := runtime_replay_payload.

Theorem replay_frozen_runtime_equivalence :
  replay_frozen_payload = replay_runtime_payload.
Proof. exact frozen_runtime_replay_payload_equal. Qed.

(** ================================================================
    Part C:  Coq trace -> projected height totality
    ================================================================ *)

Definition coq_replay_height (tr : consensus_trace) : Z := trace_height tr.

Definition replay_projection_status (tr : consensus_trace) : ReplayEquivResult :=
  match project_replay tr with
  | Some _ => RER_Resolved
  | None => RER_Missing
  end.

Lemma replay_projection_status_always_resolved :
  forall tr, replay_projection_status tr = RER_Resolved.
Proof.
  intro tr. unfold replay_projection_status.
  rewrite project_replay_is_some_trace_height. reflexivity.
Qed.

(** ================================================================
    Part D:  Cross-domain bridge (Coq height vs frozen/runtime hash)
    ================================================================ *)

Definition replay_semantic_vs_frozen_result (h : Z) : ReplayEquivResult :=
  if Z.eqb h replay_frozen_payload then RER_Resolved else RER_Disagreement.

Definition replay_semantic_vs_runtime_result (h : Z) : ReplayEquivResult :=
  if Z.eqb h replay_runtime_payload then RER_Resolved else RER_Disagreement.

(** Sample Coq replay traces (in-domain inputs only; no runtime/frozen reuse). *)
Definition replay_trace_empty : consensus_trace := [].
Definition replay_trace_one : consensus_trace := [TrAcceptBlock 1].
Definition replay_trace_three : consensus_trace := replay_trace.
Definition replay_trace_five : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3;
    TrAcceptBlock 4; TrAcceptBlock 5 ].

Definition sample_replay_traces : list consensus_trace :=
  [ replay_trace_empty;
    replay_trace_one;
    replay_trace_three;
    replay_trace_five ].

Lemma sample_replay_traces_count : length sample_replay_traces = 4%nat.
Proof. reflexivity. Qed.

(** Per-trace Coq heights are small (0/1/3/5). *)
Lemma coq_replay_height_empty : coq_replay_height replay_trace_empty = 0%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_replay_height_one : coq_replay_height replay_trace_one = 1%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_replay_height_three : coq_replay_height replay_trace_three = 3%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_replay_height_five : coq_replay_height replay_trace_five = 5%Z.
Proof. vm_compute. reflexivity. Qed.

(** Honest: no sample Coq replay height equals the 256-bit frozen replay payload. *)
Definition any_sample_height_matches_frozen : bool :=
  existsb (fun tr => Z.eqb (coq_replay_height tr) replay_frozen_payload)
          sample_replay_traces.

Lemma any_sample_height_matches_frozen_false :
  any_sample_height_matches_frozen = false.
Proof. vm_compute. reflexivity. Qed.

Definition any_sample_height_matches_runtime : bool :=
  existsb (fun tr => Z.eqb (coq_replay_height tr) replay_runtime_payload)
          sample_replay_traces.

Lemma any_sample_height_matches_runtime_false :
  any_sample_height_matches_runtime = false.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_semantic_vs_frozen_all_disagree :
  forall tr, In tr sample_replay_traces ->
    replay_semantic_vs_frozen_result (coq_replay_height tr) = RER_Disagreement.
Proof.
  intros tr Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst tr; reflexivity | ]).
  contradiction.
Qed.

Lemma replay_semantic_vs_runtime_all_disagree :
  forall tr, In tr sample_replay_traces ->
    replay_semantic_vs_runtime_result (coq_replay_height tr) = RER_Disagreement.
Proof.
  intros tr Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst tr; reflexivity | ]).
  contradiction.
Qed.

(** ================================================================
    Part E:  Replay component verdict (PARTIAL — projection only)
    ================================================================ *)

Definition replay_equivalence_projection_verified : bool := true.

Definition replay_equivalence_cross_domain_closed : bool := false.

Definition replay_equivalence_status_partial : bool :=
  replay_equivalence_projection_verified &&
  negb replay_equivalence_cross_domain_closed.

Lemma replay_equivalence_status_partial_value :
  replay_equivalence_status_partial = true.
Proof. vm_compute. reflexivity. Qed.

(** Global equivalence remains false; only Replay projection layer is recorded. *)
Lemma replay_does_not_establish_global_equivalence :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

(** Re-export of the 012Z parametric encoder commutativity (kept here for
    catalog cross-reference; NOT a raw equality between height and hash). *)
Theorem replay_state_encoding_commutativity_reexport :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_replay tr = Some h ->
    f h = replay_runtime_payload ->
    f h = replay_frozen_payload.
Proof. exact replay_state_encoding_correct. Qed.
