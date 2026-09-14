(** Phase4FConsensus/ConsensusVectorRegistry.v
    Registry of established golden-vector families (PHASE-4F-001B).

    Counts align with [formal_verification/phase3_golden_vectors.json] and
    PHASE-4A/4B/4D/4E audit docs. Phase-4F trace execution is scaffolding only. *)

From Coq Require Import List.
Import ListNotations.

(** Vector family tags (one row per required TASK category). *)
Inductive VectorFamily :=
  | VF_PHASE3_CR
  | VF_PHASE3_SIG
  | VF_PHASE3_WIN
  | VF_PHASE4A_DRAW
  | VF_PHASE4B_REPLAY
  | VF_PHASE4D_SUPPLY
  | VF_PHASE4E_CONNECTBLOCK.

(** Whether the family has been driven through Phase-4F trace execution. *)
Inductive FamilyExecutionStatus :=
  | FES_NotExecuted
  | FES_Executed
  | FES_Partial.

(** Coverage vs golden oracle (prior PHASE-3/4A Coq kernel runs). *)
Inductive FamilyCoverageStatus :=
  | FCS_MATCH
  | FCS_PARTIAL
  | FCS_BLOCKED
  | FCS_PLACEHOLDER.

Record VectorFamilyEntry := {
  vfe_family : VectorFamily;
  vfe_vector_count : nat;
  vfe_execution_status : FamilyExecutionStatus;
  vfe_coverage_status : FamilyCoverageStatus;
}.

(** These counts are maintained by hand and must equal what
    [tools/generate_phase4f_golden_traces.py] emits from the golden-vector JSON;
    [ConsensusTraceExecution.registry_execution_count_eq_total] is what forces the two to agree.

    They stood at 9 / 5 / 7 / 25 (total 73) against the 57-vector baseline JSON. The JSON later
    grew to 324 vectors without the generated data file being rebuilt — the rebuild had started
    emitting invalid Coq after the `SigmaCR01` -> `SigmaPoW01` rename, so nobody could run it —
    and the equality proof never fired because both sides stayed frozen together. Counts below
    are the current JSON: ConvergenceRoot 25, SigmaPoW01 50, RunSelection 84, and the draw family
    as the union of those three with Inequality 25. *)

(** [PHASE4B_REPLAY]: 3 Replay + 12 ReorgBind JSON vectors (ReplayReorgCore scope). *)
Definition count_PHASE4B_REPLAY : nat := 15.

Definition vector_registry : list VectorFamilyEntry :=
  {| vfe_family := VF_PHASE3_CR;
     vfe_vector_count := 25;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_PLACEHOLDER;
  |}
  ::
  {| vfe_family := VF_PHASE3_SIG;
     vfe_vector_count := 50;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_PLACEHOLDER;
  |}
  ::
  {| vfe_family := VF_PHASE3_WIN;
     vfe_vector_count := 84;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_PLACEHOLDER;
  |}
  ::
  {| vfe_family := VF_PHASE4A_DRAW;
     vfe_vector_count := 184;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_MATCH;
  |}
  ::
  {| vfe_family := VF_PHASE4B_REPLAY;
     vfe_vector_count := count_PHASE4B_REPLAY;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_BLOCKED;
  |}
  ::
  {| vfe_family := VF_PHASE4D_SUPPLY;
     vfe_vector_count := 5;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_BLOCKED;
  |}
  ::
  {| vfe_family := VF_PHASE4E_CONNECTBLOCK;
     vfe_vector_count := 7;
     vfe_execution_status := FES_Partial;
     vfe_coverage_status := FCS_PLACEHOLDER;
  |}
  :: nil.

Definition registry_total_vector_count : nat :=
  fold_right (fun e acc => vfe_vector_count e + acc) 0 vector_registry.

Lemma registry_total_vector_count_eq_370 :
  registry_total_vector_count = 370.
Proof.
  reflexivity.
Qed.

Definition eq_VectorFamily (a b : VectorFamily) : bool :=
  match a, b with
  | VF_PHASE3_CR, VF_PHASE3_CR => true
  | VF_PHASE3_SIG, VF_PHASE3_SIG => true
  | VF_PHASE3_WIN, VF_PHASE3_WIN => true
  | VF_PHASE4A_DRAW, VF_PHASE4A_DRAW => true
  | VF_PHASE4B_REPLAY, VF_PHASE4B_REPLAY => true
  | VF_PHASE4D_SUPPLY, VF_PHASE4D_SUPPLY => true
  | VF_PHASE4E_CONNECTBLOCK, VF_PHASE4E_CONNECTBLOCK => true
  | _, _ => false
  end.

Definition lookup_family_entry (fam : VectorFamily) : option VectorFamilyEntry :=
  find (fun e => eq_VectorFamily (vfe_family e) fam) vector_registry.

Definition family_vector_count (fam : VectorFamily) : nat :=
  match lookup_family_entry fam with
  | Some e => vfe_vector_count e
  | None => 0
  end.
