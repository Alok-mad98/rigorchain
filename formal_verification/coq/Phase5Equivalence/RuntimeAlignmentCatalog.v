(** Phase5Equivalence/RuntimeAlignmentCatalog.v
    Initial Coq = Runtime alignment (TASK-018GG-K) — THEORETICAL LAYER ONLY.

    Maps each PotentiallyClosable component x minimal witness field x
    observation source to a field-level alignment status. Relates 018GG-C
    [CanonicalStateHash] projections to frozen Runtime evidence metadata
    without asserting equivalence.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT assert global Coq = Runtime equivalence.
    - DOES NOT close PD-4F-OB-02 or PD-4F-CPP-01.
    - No SHA-256 / runtime / consensus code modified.
    - Reuses only 018GG-A/B/C/D/E/F/G/H/I/J artifacts (no fabricated payloads). *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.RuntimeWitnessRequirementCatalog.

Close Scope Z_scope.

(** ================================================================
    Part A:  Alignment status vocabulary
    ================================================================ *)

Inductive FieldAlignmentStatus :=
| FAS_Aligned
| FAS_Missing
| FAS_PartiallyAligned.

Definition fas_aligned (s : FieldAlignmentStatus) : bool :=
  match s with FAS_Aligned => true | _ => false end.
Definition fas_missing (s : FieldAlignmentStatus) : bool :=
  match s with FAS_Missing => true | _ => false end.
Definition fas_partial (s : FieldAlignmentStatus) : bool :=
  match s with FAS_PartiallyAligned => true | _ => false end.

(** Component-level summary (018GG-K classification). *)
Inductive ComponentAlignmentStatus :=
| CAS_Aligned
| CAS_PartialAlignment
| CAS_FundamentallyBlocked.

Definition cas_aligned (s : ComponentAlignmentStatus) : bool :=
  match s with CAS_Aligned => true | _ => false end.
Definition cas_partial (s : ComponentAlignmentStatus) : bool :=
  match s with CAS_PartialAlignment => true | _ => false end.
Definition cas_blocked (s : ComponentAlignmentStatus) : bool :=
  match s with CAS_FundamentallyBlocked => true | _ => false end.

(** ================================================================
    Part B:  Machine-derivability (Coq model only; no runtime import)
    ================================================================ *)

(** A witness field is Coq-machine-derivable when 018GG-A already extracts
    it from [consensus_trace] without importing runtime bytes. *)
Definition coq_witness_field_derived (f : RuntimeWitnessField) : bool :=
  match f with
  | RWF_HashKernelOutput => false
  | _ => true
  end.

(** Runtime witness export present in frozen evidence (018GG-J: all false). *)
Definition runtime_witness_field_available (f : RuntimeWitnessField) : bool :=
  false.

(** Classify one cell from derivability flags (no hash equality). *)
Definition classify_field_alignment
  (coq_der runtime_avail : bool) : FieldAlignmentStatus :=
  if coq_der then
    if runtime_avail then FAS_Aligned else FAS_PartiallyAligned
  else
    if runtime_avail then FAS_PartiallyAligned else FAS_Missing.

(** ================================================================
    Part C:  Per-cell catalog entry (component x field x source)
    ================================================================ *)

Record RuntimeAlignmentEntry := {
  rae_component : Component;
  rae_field : RuntimeWitnessField;
  rae_source : HashSource;
  rae_status : FieldAlignmentStatus;
  rae_coq_derived : bool;
  rae_runtime_available : bool;
}.

Definition mk_alignment_entry
  (c : Component) (f : RuntimeWitnessField) (src : HashSource)
  : RuntimeAlignmentEntry :=
  let coq_d := coq_witness_field_derived f in
  let rt_a := runtime_witness_field_available f in
  let st :=
    if source_eqb src HS_CoqAbstract
    then classify_field_alignment coq_d rt_a
    else classify_field_alignment false rt_a
  in
  {| rae_component := c;
     rae_field := f;
     rae_source := src;
     rae_status := st;
     rae_coq_derived := coq_d;
     rae_runtime_available := rt_a |}.

(** Expand one component's minimal witness set across two sources. *)
Definition alignment_entries_for_component (c : Component)
  (fields : list RuntimeWitnessField) : list RuntimeAlignmentEntry :=
  let per_field src :=
    map (fun f => mk_alignment_entry c f src) fields
  in
  per_field HS_CoqAbstract ++ per_field HS_Runtime.

Definition replay_alignment_entries : list RuntimeAlignmentEntry :=
  alignment_entries_for_component C_Replay replay_cb_minimal_witnesses.

Definition reorg_alignment_entries : list RuntimeAlignmentEntry :=
  alignment_entries_for_component C_Reorg reorg_minimal_witnesses.

Definition connectblock_alignment_entries : list RuntimeAlignmentEntry :=
  alignment_entries_for_component C_ConnectBlock replay_cb_minimal_witnesses.

Definition runtime_alignment_catalog : list RuntimeAlignmentEntry :=
  replay_alignment_entries ++
  reorg_alignment_entries ++
  connectblock_alignment_entries.

Definition runtime_alignment_catalog_count : nat :=
  length runtime_alignment_catalog.

(** ================================================================
    Part D:  Hash-projection alignment (Coq abstract hash vs runtime metadata)
    ================================================================ *)

Record HashProjectionAlignment := {
  hpa_component : Component;
  hpa_coq_hash : CanonicalStateHash;
  hpa_runtime_hash : Z;
  hpa_field_status : FieldAlignmentStatus;
}.

Definition replay_hash_projection : HashProjectionAlignment :=
  {| hpa_component := C_Replay;
     hpa_coq_hash := hash_replay_state replay_trace;
     hpa_runtime_hash := rer_block_hash ev_005_multinode;
     hpa_field_status := FAS_PartiallyAligned |}.

Definition reorg_hash_projection : HashProjectionAlignment :=
  {| hpa_component := C_Reorg;
     hpa_coq_hash := hash_reorg_state reorg_trace;
     hpa_runtime_hash := rer_block_hash ev_005_reorg;
     hpa_field_status := FAS_PartiallyAligned |}.

Definition connectblock_hash_projection : HashProjectionAlignment :=
  {| hpa_component := C_ConnectBlock;
     hpa_coq_hash := hash_connectblock_state [ TrAcceptBlock 7 ];
     hpa_runtime_hash := rer_block_hash ev_005_connect;
     hpa_field_status := FAS_PartiallyAligned |}.

Definition hash_projection_alignments : list HashProjectionAlignment :=
  [ replay_hash_projection; reorg_hash_projection; connectblock_hash_projection ].

(** Hash projections are recorded but NOT equated to runtime block hashes. *)
Lemma hash_projection_not_runtime_equal :
  hash_replay_state replay_trace <> rer_block_hash ev_005_multinode /\
  hash_reorg_state reorg_trace <> rer_block_hash ev_005_reorg /\
  hash_connectblock_state [ TrAcceptBlock 7 ] <> rer_block_hash ev_005_connect.
Proof. vm_compute. repeat split; discriminate. Qed.

(** ================================================================
    Part E:  Component-level alignment summary
    ================================================================ *)

Record ComponentAlignmentSummary := {
  cas_component : Component;
  cas_status : ComponentAlignmentStatus;
  cas_minimal_witness_count : nat;
  cas_coq_derived_count : nat;
  cas_runtime_available_count : nat;
  cas_fields_to_reach_ready : list RuntimeWitnessField;
  cas_closure_theoretically_possible : bool;
}.

Definition count_coq_derived_in (fields : list RuntimeWitnessField) : nat :=
  length (filter coq_witness_field_derived fields).

Definition replay_component_summary : ComponentAlignmentSummary :=
  {| cas_component := C_Replay;
     cas_status := CAS_PartialAlignment;
     cas_minimal_witness_count := 5%nat;
     cas_coq_derived_count := 4%nat;
     cas_runtime_available_count := 0%nat;
     cas_fields_to_reach_ready := replay_cb_minimal_witnesses;
     cas_closure_theoretically_possible := true |}.

Definition reorg_component_summary : ComponentAlignmentSummary :=
  {| cas_component := C_Reorg;
     cas_status := CAS_PartialAlignment;
     cas_minimal_witness_count := 6%nat;
     cas_coq_derived_count := 5%nat;
     cas_runtime_available_count := 0%nat;
     cas_fields_to_reach_ready := reorg_minimal_witnesses;
     cas_closure_theoretically_possible := true |}.

Definition connectblock_component_summary : ComponentAlignmentSummary :=
  {| cas_component := C_ConnectBlock;
     cas_status := CAS_PartialAlignment;
     cas_minimal_witness_count := 5%nat;
     cas_coq_derived_count := 4%nat;
     cas_runtime_available_count := 0%nat;
     cas_fields_to_reach_ready := replay_cb_minimal_witnesses;
     cas_closure_theoretically_possible := true |}.

Definition draw_component_summary : ComponentAlignmentSummary :=
  {| cas_component := C_Draw;
     cas_status := CAS_FundamentallyBlocked;
     cas_minimal_witness_count := 0%nat;
     cas_coq_derived_count := 0%nat;
     cas_runtime_available_count := 0%nat;
     cas_fields_to_reach_ready := [];
     cas_closure_theoretically_possible := false |}.

Definition component_alignment_summaries : list ComponentAlignmentSummary :=
  [ replay_component_summary;
    reorg_component_summary;
    connectblock_component_summary;
    draw_component_summary ].

(** ================================================================
    Part F:  Field-level counts
    ================================================================ *)

Definition alignment_statuses : list FieldAlignmentStatus :=
  map rae_status runtime_alignment_catalog.

Definition alignment_aligned_count : nat :=
  length (filter fas_aligned alignment_statuses).
Definition alignment_missing_count : nat :=
  length (filter fas_missing alignment_statuses).
Definition alignment_partial_count : nat :=
  length (filter fas_partial alignment_statuses).

Definition alignment_total_fields : nat := runtime_alignment_catalog_count.

Lemma alignment_total_fields_value : alignment_total_fields = 32%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_aligned_count_value : alignment_aligned_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_partial_count_value : alignment_partial_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_missing_count_value : alignment_missing_count = 19%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_alignment_partition_complete :
  alignment_aligned_count + alignment_missing_count + alignment_partial_count
  = alignment_total_fields.
Proof. vm_compute. reflexivity. Qed.

(** Per-component minimal / available / missing (re-export 018GG-J). *)
Lemma replay_minimal_available_missing :
  length replay_cb_minimal_witnesses = 5%nat /\
  length replay_witness_requirement.(rwr_currently_available) = 0%nat /\
  length replay_witness_requirement.(rwr_missing) = 5%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma reorg_minimal_available_missing :
  length reorg_minimal_witnesses = 6%nat /\
  length reorg_witness_requirement.(rwr_currently_available) = 0%nat /\
  length reorg_witness_requirement.(rwr_missing) = 6%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma connectblock_minimal_available_missing :
  length replay_cb_minimal_witnesses = 5%nat /\
  length connectblock_witness_requirement.(rwr_currently_available) = 0%nat /\
  length connectblock_witness_requirement.(rwr_missing) = 5%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

(** ================================================================
    Part G:  Required proofs
    ================================================================ *)

Lemma runtime_alignment_catalog_nonempty :
  runtime_alignment_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_alignment_status_defined :
  forall e, In e runtime_alignment_catalog ->
    match e.(rae_status) with
    | FAS_Aligned => True
    | FAS_Missing => True
    | FAS_PartiallyAligned => True
    end.
Proof.
  intros e Hin. destruct e.(rae_status); exact I.
Qed.

(** ================================================================
    Part H:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_alignment_layer_established : bool := true.
Definition runtime_alignment_coq_eq_runtime : bool := false.

Lemma runtime_alignment_layer_established_value :
  runtime_alignment_layer_established = true.
Proof. reflexivity. Qed.

Lemma runtime_alignment_does_not_establish_equivalence :
  runtime_alignment_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_alignment_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

Lemma draw_fundamentally_blocked :
  draw_component_summary.(cas_status) = CAS_FundamentallyBlocked.
Proof. reflexivity. Qed.

Lemma replay_reorg_cb_partial_alignment :
  replay_component_summary.(cas_status) = CAS_PartialAlignment /\
  reorg_component_summary.(cas_status) = CAS_PartialAlignment /\
  connectblock_component_summary.(cas_status) = CAS_PartialAlignment.
Proof. repeat split; reflexivity. Qed.
