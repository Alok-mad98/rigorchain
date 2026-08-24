(** Phase4FConsensus/ConsensusGoldenTrace.v
    Golden consensus-trace data model (PHASE-4F-001B / 001C).

    JSON golden vectors decode to [TraceCase] via [mk_golden_trace_case] in
    [ConsensusGoldenVectorsData.v] (generated). No draw kernel / replay imports. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.

(** Outcome of running one trace case through [execute_trace]. *)
Inductive TraceExecutionStatus :=
  | TES_NotRun
  | TES_RanMatch
  | TES_RanPartial
  | TES_RanBlocked
  | TES_RanError.

(** Inputs for a single golden trace case. *)
Record TraceInput := {
  ti_init : ConsensusState;
  ti_trace : consensus_trace;
}.

(** Expected outcome (golden reference slot; filled by later TASKs). *)
Record TraceExpected := {
  te_final : ConsensusState;
  te_status : TraceExecutionStatus;
}.

(** Decoded JSON / harness metadata (no byte payload in consensus machine).

    [gvm_golden_winner] / [gvm_kernel_winner] are the RunSelection winner indices
    (-1 = no winner). The kernel winner is sourced from the PHASE-4A2 verified
    Coq draw-kernel execution evidence (not recomputed here). *)
Record GoldenVectorMeta := {
  gvm_slot : nat;
  gvm_tx_count : nat;
  gvm_semantic_blocked : bool;
  gvm_is_draw_winner : bool;
  gvm_golden_winner : Z;
  gvm_kernel_winner : Z;
}.

(** Golden match predicate (draw winner projection; NOT full consensus equivalence). *)
Definition meta_golden_match (m : GoldenVectorMeta) : bool :=
  andb m.(gvm_is_draw_winner) (Z.eqb m.(gvm_golden_winner) m.(gvm_kernel_winner)).

(** PHASE-4F-001E independent re-computation inputs.

    [dc_eligible] is imported kernel evidence (the sigma threshold needs the hash kernel, which
    is out of Phase-4F scope). [dc_sigma] / [dc_weight] / [dc_txid] are raw registry inputs used
    by the independent selection re-computation.

    [dc_sigma] was absent, and its absence was not a simplification but a defect: without sigma
    the audit could only rank candidates by weight, whereas the protocol ranks them by the sigma
    cross-product (Selection.v [pick_better_tx]). The weight ordering happened to coincide with
    the real ordering on the 57-vector baseline, so [draw_circularity_closed] passed while
    proving nothing about the actual selection rule; against the current vector set the two
    orderings disagree on half the rows. Importing sigma is no more circular than importing
    [dc_eligible], which is already derived from it.

    Integer encodings match the C++/Python conventions exactly, and the tie-break depends on
    getting them right: [dc_sigma] is the sigma digest as [arith_uint256], i.e. LITTLE-endian over
    the 32 bytes, and [dc_txid] is the txid under [uint256::SetHex] byte order, which is the
    little-endian reading of the JSON hex — byte-wise memcmp on the reversed buffer is the same
    order as integer comparison of that value. *)
Record DrawCandidate := {
  dc_eligible : bool;
  dc_sigma : Z;
  dc_weight : Z;
  dc_txid : Z;
}.

(** One audit row per draw-applicable trace slot (RunSelection x family). *)
Record DrawAuditRow := {
  dar_vector_id : nat;
  dar_family_tag : nat;
  dar_imported_winner : Z;
  dar_candidates : list DrawCandidate;
}.

(** One executable trace case (links to external vector id via [tc_vector_id]). *)
Record TraceCase := {
  tc_case_id : nat;
  tc_vector_id : nat;
  tc_family_tag : nat;
  tc_meta : GoldenVectorMeta;
  tc_input : TraceInput;
  tc_expected : TraceExpected;
}.

(** Result of replaying [TraceInput] through the Phase-4F consensus machine. *)
Record ReplayResult := {
  rr_status : TraceExecutionStatus;
  rr_final : option ConsensusState;
}.

(** Combined golden comparison slot (equivalence check deferred). *)
Record GoldenTraceResult := {
  gtr_case_id : nat;
  gtr_replay : ReplayResult;
  gtr_golden_match : bool;
}.

(** Build [consensus_trace] from decoded tx/step count (abstract [TrAcceptBlock] chain). *)
Fixpoint trace_from_tx_count (base count : nat) : consensus_trace :=
  match count with
  | O => nil
  | S n' => TrAcceptBlock (base + n' - 1) :: trace_from_tx_count base n'
  end.

Definition expected_status_for_meta (m : GoldenVectorMeta) : TraceExecutionStatus :=
  if m.(gvm_semantic_blocked)
  then TES_RanBlocked
  else TES_NotRun.

Definition consensus_state_after_trace (inp : TraceInput) : ConsensusState :=
  match execute_trace inp.(ti_init) inp.(ti_trace) with
  | None => inp.(ti_init)
  | Some s => s
  end.

(** Constructor used by generated [ConsensusGoldenVectorsData.v]. *)
Definition mk_golden_trace_case
  (case_id vector_id family_tag slot tx_count : nat)
  (semantic_blocked is_draw_winner : bool)
  (golden_winner kernel_winner : Z) : TraceCase :=
  let meta :=
    {| gvm_slot := slot;
       gvm_tx_count := tx_count;
       gvm_semantic_blocked := semantic_blocked;
       gvm_is_draw_winner := is_draw_winner;
       gvm_golden_winner := golden_winner;
       gvm_kernel_winner := kernel_winner;
    |} in
  let inp :=
    {| ti_init := init_consensus_state;
       ti_trace := trace_from_tx_count slot tx_count;
    |} in
  let exp_status := expected_status_for_meta meta in
  {| tc_case_id := case_id;
     tc_vector_id := vector_id;
     tc_family_tag := family_tag;
     tc_meta := meta;
     tc_input := inp;
     tc_expected :=
        {| te_final := consensus_state_after_trace inp;
           te_status := exp_status;
        |};
  |}.

(** Default empty trace case (scaffolding). *)
Definition default_trace_case : TraceCase :=
  mk_golden_trace_case 0 0 0 0 1 false false (-1)%Z (-1)%Z.

(** Map machine [option] to execution status (no golden compare yet). *)
Definition status_of_option (o : option ConsensusState) : TraceExecutionStatus :=
  match o with
  | Some _ => TES_RanPartial
  | None => TES_RanError
  end.

Definition replay_result_of_trace (inp : TraceInput) : ReplayResult :=
  let o := execute_trace inp.(ti_init) inp.(ti_trace) in
  {| rr_status := status_of_option o;
     rr_final := o;
  |}.

(** PHASE-4F-001D: [gtr_golden_match] is now derived from the draw winner
    projection ([meta_golden_match]); it is NO LONGER a hardcoded [false]. *)
Definition golden_trace_result_of_case (c : TraceCase) : GoldenTraceResult :=
  let rr := replay_result_of_trace (tc_input c) in
  {| gtr_case_id := tc_case_id c;
     gtr_replay := rr;
     gtr_golden_match := meta_golden_match (tc_meta c);
  |}.

(** [gtr_golden_match] equals the meta projection predicate (no constant false). *)
Lemma golden_trace_result_match_is_meta (c : TraceCase) :
  (golden_trace_result_of_case c).(gtr_golden_match) = meta_golden_match (tc_meta c).
Proof.
  reflexivity.
Qed.
