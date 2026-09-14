(** Phase4FConsensus/ConsensusDrawProjection.v
    Draw winner projection (PHASE-4F-001D).

    Projects each [TraceCase] to a [DrawProjectionResult] by comparing the golden
    RunSelection winner index against the PHASE-4A2-verified Coq draw-kernel winner
    (embedded in [GoldenVectorMeta]). This is a SEMANTIC PROJECTION of the already
    proven draw domain only — NOT full consensus equivalence, and NO kernel/RFC/
    C++ code is touched. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.

(** REQUIRED-2: projection status. *)
Inductive DrawProjectionStatus :=
  | DPS_Match
  | DPS_Mismatch
  | DPS_NotApplicable.

Record DrawProjectionResult := {
  dpr_case_id : nat;
  dpr_vector_id : nat;
  dpr_golden_winner : Z;
  dpr_coq_winner : Z;
  dpr_status : DrawProjectionStatus;
}.

(** REQUIRED-4: compare a (golden, coq) winner pair under draw applicability. *)
Definition compare_projection_to_golden
  (is_draw : bool) (golden coq_winner : Z) : DrawProjectionStatus :=
  if is_draw
  then (if Z.eqb golden coq_winner then DPS_Match else DPS_Mismatch)
  else DPS_NotApplicable.

(** REQUIRED-1 / REQUIRED-3: project winner + selection status from kernel evidence. *)
Definition project_draw_result (c : TraceCase) : DrawProjectionResult :=
  let m := tc_meta c in
  {| dpr_case_id := tc_case_id c;
     dpr_vector_id := tc_vector_id c;
     dpr_golden_winner := gvm_golden_winner m;
     dpr_coq_winner := gvm_kernel_winner m;
     dpr_status :=
        compare_projection_to_golden
          (gvm_is_draw_winner m) (gvm_golden_winner m) (gvm_kernel_winner m);
  |}.

(** Bridge: projection [DPS_Match] iff [meta_golden_match] (used by GoldenTraceResult). *)
Definition projection_golden_match (c : TraceCase) : bool :=
  match dpr_status (project_draw_result c) with
  | DPS_Match => true
  | _ => false
  end.

Lemma projection_golden_match_eq_meta (c : TraceCase) :
  projection_golden_match c = meta_golden_match (tc_meta c).
Proof.
  unfold projection_golden_match, project_draw_result, meta_golden_match,
          compare_projection_to_golden.
  destruct (gvm_is_draw_winner (tc_meta c)) eqn:Hd; simpl.
  - destruct (Z.eqb (gvm_golden_winner (tc_meta c)) (gvm_kernel_winner (tc_meta c)));
      reflexivity.
  - reflexivity.
Qed.

(** Projection status aggregation helpers (used by audit reporting). *)
Definition is_match (r : DrawProjectionResult) : bool :=
  match dpr_status r with DPS_Match => true | _ => false end.

Definition is_mismatch (r : DrawProjectionResult) : bool :=
  match dpr_status r with DPS_Mismatch => true | _ => false end.

Definition is_not_applicable (r : DrawProjectionResult) : bool :=
  match dpr_status r with DPS_NotApplicable => true | _ => false end.

Definition count_status (f : DrawProjectionResult -> bool)
  (rs : list DrawProjectionResult) : nat :=
  length (filter f rs).

(** For draw-applicable vectors the projection never yields [DPS_NotApplicable]. *)
Lemma draw_applicable_not_na (c : TraceCase) :
  gvm_is_draw_winner (tc_meta c) = true ->
  dpr_status (project_draw_result c) <> DPS_NotApplicable.
Proof.
  intros Hd. unfold project_draw_result, compare_projection_to_golden; simpl.
  rewrite Hd.
  destruct (Z.eqb (gvm_golden_winner (tc_meta c)) (gvm_kernel_winner (tc_meta c)));
    discriminate.
Qed.
