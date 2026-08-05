(** Phase5Equivalence/ReorgEquivalence.v
    Reorg equivalence proofs (TASK-018FF-D).

    Records the Reorg equivalence projection chain:

      Coq reorg trace  --[execute_trace + height_of]-->  coq_reorg_height (Z)
                                                                |
                            (height vs 256-bit hash domain mismatch)
                                                                v
      frozen reorg payload (TASK-005 fork-block hash)  =  runtime reorg payload

    HONESTY BOUNDARY:
    - Frozen-vs-runtime payload identity is discharged (re-export of 013AA).
    - Coq projection [project_reorg] is total: every trace projects to Some height.
    - Coq semantic payload (post-rollback height) does NOT match the 256-bit
      frozen/runtime payload — recorded as DISAGREEMENT, not silently bridged.
    - The encoder used here ([trace_height]) is a placeholder identity-style
      projection (height domain only); marks Reorg PARTIAL.
    - Draw, Replay, ConnectBlock, Supply are untouched.
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
Require Import Rigor.Phase4GRefinement.ReorgStateEncodingTheorem.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.

(** ================================================================
    Part A:  Reorg equivalence result vocabulary
    ================================================================ *)

Inductive ReorgEquivKind :=
| RoEK_FrozenRuntimeIdentity
| RoEK_ProjectionTotality
| RoEK_SemanticVsFrozen
| RoEK_SemanticVsRuntime.

Inductive ReorgEquivResult :=
| RoER_Resolved
| RoER_Disagreement
| RoER_Missing
| RoER_Partial.

Definition reorg_result_resolved (r : ReorgEquivResult) : bool :=
  match r with
  | RoER_Resolved => true
  | _ => false
  end.

Definition reorg_result_disagreement (r : ReorgEquivResult) : bool :=
  match r with
  | RoER_Disagreement => true
  | _ => false
  end.

Definition reorg_result_missing (r : ReorgEquivResult) : bool :=
  match r with
  | RoER_Missing => true
  | _ => false
  end.

Definition reorg_result_partial (r : ReorgEquivResult) : bool :=
  match r with
  | RoER_Partial => true
  | _ => false
  end.

(** [project_reorg] is [Some (trace_height tr)] — definitionally an identity
    wrapper over the post-rollback height field. Marks the projection layer
    PARTIAL. *)
Definition reorg_projection_uses_identity_map : bool := true.

Lemma project_reorg_is_some_trace_height :
  forall tr, project_reorg tr = Some (trace_height tr).
Proof. intros tr. reflexivity. Qed.

(** ================================================================
    Part B:  Frozen <-> Runtime payload identity (TASK-005 fork hash)
    ================================================================ *)

Definition reorg_frozen_payload : Z := frozen_reorg_payload.
Definition reorg_runtime_payload : Z := runtime_reorg_payload.

Theorem reorg_frozen_runtime_equivalence :
  reorg_frozen_payload = reorg_runtime_payload.
Proof. exact frozen_runtime_reorg_payload_equal. Qed.

(** ================================================================
    Part C:  Coq trace -> projected post-rollback height totality
    ================================================================ *)

Definition coq_reorg_height (tr : consensus_trace) : Z := trace_height tr.

Definition reorg_projection_status (tr : consensus_trace) : ReorgEquivResult :=
  match project_reorg tr with
  | Some _ => RoER_Resolved
  | None => RoER_Missing
  end.

Lemma reorg_projection_status_always_resolved :
  forall tr, reorg_projection_status tr = RoER_Resolved.
Proof.
  intro tr. unfold reorg_projection_status.
  rewrite project_reorg_is_some_trace_height. reflexivity.
Qed.

(** ================================================================
    Part D:  Cross-domain bridge (Coq height vs frozen/runtime fork hash)
    ================================================================ *)

Definition reorg_semantic_vs_frozen_result (h : Z) : ReorgEquivResult :=
  if Z.eqb h reorg_frozen_payload then RoER_Resolved else RoER_Disagreement.

Definition reorg_semantic_vs_runtime_result (h : Z) : ReorgEquivResult :=
  if Z.eqb h reorg_runtime_payload then RoER_Resolved else RoER_Disagreement.

(** Sample Coq reorg traces (each ends with a rollback; in-domain inputs only). *)
Definition reorg_trace_one : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrRollback 1 ].

Definition reorg_trace_two : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3; TrRollback 1 ].

Definition reorg_trace_three : consensus_trace := reorg_trace.

Definition reorg_trace_four : consensus_trace :=
  [ TrAcceptBlock 1; TrAcceptBlock 2; TrAcceptBlock 3;
    TrAcceptBlock 4; TrAcceptBlock 5; TrRollback 1 ].

Definition sample_reorg_traces : list consensus_trace :=
  [ reorg_trace_one;
    reorg_trace_two;
    reorg_trace_three;
    reorg_trace_four ].

Lemma sample_reorg_traces_count : length sample_reorg_traces = 4%nat.
Proof. reflexivity. Qed.

(** Per-trace post-rollback heights are small (1/2/3/4). *)
Lemma coq_reorg_height_one : coq_reorg_height reorg_trace_one = 1%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_reorg_height_two : coq_reorg_height reorg_trace_two = 2%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_reorg_height_three : coq_reorg_height reorg_trace_three = 3%Z.
Proof. vm_compute. reflexivity. Qed.

Lemma coq_reorg_height_four : coq_reorg_height reorg_trace_four = 4%Z.
Proof. vm_compute. reflexivity. Qed.

(** Honest: no sample Coq reorg height equals the 256-bit frozen reorg payload. *)
Definition any_sample_height_matches_frozen : bool :=
  existsb (fun tr => Z.eqb (coq_reorg_height tr) reorg_frozen_payload)
          sample_reorg_traces.

Lemma any_sample_height_matches_frozen_false :
  any_sample_height_matches_frozen = false.
Proof. vm_compute. reflexivity. Qed.

Definition any_sample_height_matches_runtime : bool :=
  existsb (fun tr => Z.eqb (coq_reorg_height tr) reorg_runtime_payload)
          sample_reorg_traces.

Lemma any_sample_height_matches_runtime_false :
  any_sample_height_matches_runtime = false.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_semantic_vs_frozen_all_disagree :
  forall tr, In tr sample_reorg_traces ->
    reorg_semantic_vs_frozen_result (coq_reorg_height tr) = RoER_Disagreement.
Proof.
  intros tr Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst tr; reflexivity | ]).
  contradiction.
Qed.

Lemma reorg_semantic_vs_runtime_all_disagree :
  forall tr, In tr sample_reorg_traces ->
    reorg_semantic_vs_runtime_result (coq_reorg_height tr) = RoER_Disagreement.
Proof.
  intros tr Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst tr; reflexivity | ]).
  contradiction.
Qed.

(** ================================================================
    Part E:  Reorg component verdict (PARTIAL — projection only)
    ================================================================ *)

Definition reorg_equivalence_projection_verified : bool := true.

Definition reorg_equivalence_cross_domain_closed : bool := false.

Definition reorg_equivalence_status_partial : bool :=
  reorg_equivalence_projection_verified &&
  negb reorg_equivalence_cross_domain_closed.

Lemma reorg_equivalence_status_partial_value :
  reorg_equivalence_status_partial = true.
Proof. vm_compute. reflexivity. Qed.

(** Global equivalence remains false; only Reorg projection layer is recorded. *)
Lemma reorg_does_not_establish_global_equivalence :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

(** Re-export of the 013AA parametric encoder commutativity (kept here for
    catalog cross-reference; NOT a raw equality between height and hash). *)
Theorem reorg_state_encoding_commutativity_reexport :
  forall (tr : consensus_trace) (h : Z) (f : Z -> Z),
    project_reorg tr = Some h ->
    f h = reorg_runtime_payload ->
    f h = reorg_frozen_payload.
Proof. exact reorg_state_encoding_correct. Qed.
