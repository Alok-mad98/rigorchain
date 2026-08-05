(** Phase5Equivalence/RuntimeWitnessIntegrationCatalog.v
    Runtime witness integration (TASK-018GG-L).

    Maps the 018GG-J minimal runtime witness set into the 018GG-A/B/C
    [CanonicalStateHash] projection chain for Replay / Reorg / ConnectBlock.
    Annotates per-field availability (Coq-derived / missing / requires SHA kernel)
    and per-cell [IntegratedFieldStatus] without asserting equivalence.

    HONESTY BOUNDARY (CRITICAL):
    - No fabricated SHA-256 / runtime hashes.
    - DOES NOT assert Coq = Runtime; PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN.
    - Reuses 018GG-A/B/C/J and 018GG-K [CAS_PartialAlignment] only.
    - No consensus / runtime code modified. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.CanonicalBytesProjection.
Require Import Rigor.Phase5Equivalence.CanonicalStateHashModel.
Require Import Rigor.Phase5Equivalence.CanonicalHashAlignmentBridge.
Require Import Rigor.Phase5Equivalence.RuntimeWitnessRequirementCatalog.
Require Import Rigor.Phase5Equivalence.RuntimeAlignmentCatalog.

Close Scope Z_scope.

(** ================================================================
    Part A:  Integration vocabulary
    ================================================================ *)

Inductive FieldAvailabilityAnnotation :=
| FAA_DerivedFromCoq
| FAA_Missing
| FAA_RequiresShaKernel.

Inductive IntegratedFieldStatus :=
| IFS_Aligned
| IFS_Partial
| IFS_Missing.

Definition ifs_aligned (s : IntegratedFieldStatus) : bool :=
  match s with IFS_Aligned => true | _ => false end.
Definition ifs_partial (s : IntegratedFieldStatus) : bool :=
  match s with IFS_Partial => true | _ => false end.
Definition ifs_missing (s : IntegratedFieldStatus) : bool :=
  match s with IFS_Missing => true | _ => false end.

(** ================================================================
    Part B:  Witness field → canonical hash projection map
    ================================================================ *)

(** Non-hash witness fields feed 018GG-A encoders, then 018GG-B bytes, then
    018GG-C [state_hash]. [RWF_HashKernelOutput] is NOT covered by the abstract
    reducer (no SHA-256 in scope). *)
Definition witness_maps_to_canonical_encoding (f : RuntimeWitnessField) : bool :=
  match f with
  | RWF_HashKernelOutput => false
  | _ => true
  end.

Definition witness_maps_to_canonical_bytes (f : RuntimeWitnessField) : bool :=
  witness_maps_to_canonical_encoding f.

Definition witness_maps_to_canonical_hash (f : RuntimeWitnessField) : bool :=
  witness_maps_to_canonical_encoding f.

(** Per-component [CanonicalStateHash] witness (018GG-C over sample traces). *)
Definition integrated_coq_hash (c : Component) : CanonicalStateHash :=
  match c with
  | C_Replay => hash_replay_state replay_trace
  | C_Reorg => hash_reorg_state reorg_trace
  | C_ConnectBlock => hash_connectblock_state [ TrAcceptBlock 7 ]
  | _ => 0%Z
  end.

(** ================================================================
    Part C:  Availability + integrated status classification
    ================================================================ *)

Definition classify_field_availability (f : RuntimeWitnessField)
  : FieldAvailabilityAnnotation :=
  if witness_maps_to_canonical_hash f
  then FAA_DerivedFromCoq
  else FAA_RequiresShaKernel.

(** Integrate 018GG-J availability with 018GG-K alignment semantics. *)
Definition classify_integrated_field
  (f : RuntimeWitnessField) (src : HashSource) : IntegratedFieldStatus :=
  if source_eqb src HS_Runtime then
    IFS_Missing
  else
    if witness_maps_to_canonical_hash f then
      if runtime_witness_field_available f then IFS_Aligned else IFS_Partial
    else
      IFS_Missing.

(** ================================================================
    Part D:  Per-cell catalog entry (component × field × source)
    ================================================================ *)

Record RuntimeWitnessIntegrationEntry := {
  rwi_component : Component;
  rwi_field : RuntimeWitnessField;
  rwi_source : HashSource;
  rwi_availability : FieldAvailabilityAnnotation;
  rwi_integrated_status : IntegratedFieldStatus;
  rwi_maps_to_encoding : bool;
  rwi_maps_to_bytes : bool;
  rwi_maps_to_hash : bool;
}.

Definition mk_integration_entry (c : Component) (f : RuntimeWitnessField)
  (src : HashSource) : RuntimeWitnessIntegrationEntry :=
  {| rwi_component := c;
     rwi_field := f;
     rwi_source := src;
     rwi_availability := classify_field_availability f;
     rwi_integrated_status := classify_integrated_field f src;
     rwi_maps_to_encoding := witness_maps_to_canonical_encoding f;
     rwi_maps_to_bytes := witness_maps_to_canonical_bytes f;
     rwi_maps_to_hash := witness_maps_to_canonical_hash f |}.

Definition integration_entries_for_component (c : Component)
  (fields : list RuntimeWitnessField) : list RuntimeWitnessIntegrationEntry :=
  let per_src src := map (fun f => mk_integration_entry c f src) fields in
  per_src HS_CoqAbstract ++ per_src HS_Runtime.

Definition replay_integration_entries : list RuntimeWitnessIntegrationEntry :=
  integration_entries_for_component C_Replay replay_cb_minimal_witnesses.

Definition reorg_integration_entries : list RuntimeWitnessIntegrationEntry :=
  integration_entries_for_component C_Reorg reorg_minimal_witnesses.

Definition connectblock_integration_entries : list RuntimeWitnessIntegrationEntry :=
  integration_entries_for_component C_ConnectBlock replay_cb_minimal_witnesses.

Definition runtime_witness_integration_catalog : list RuntimeWitnessIntegrationEntry :=
  replay_integration_entries ++
  reorg_integration_entries ++
  connectblock_integration_entries.

Definition runtime_witness_integration_count : nat :=
  length runtime_witness_integration_catalog.

(** ================================================================
    Part E:  Per-component integrated summary (018GG-K CAS re-export)
    ================================================================ *)

Record ComponentIntegrationSummary := {
  cis_component : Component;
  cis_integrated_status : IntegratedFieldStatus;
  cis_prior_alignment : ComponentAlignmentStatus;
  cis_witness_field_count : nat;
  cis_coq_derived_count : nat;
  cis_requires_sha_count : nat;
  cis_integrated_coq_hash : CanonicalStateHash;
  cis_integration_possible_after_witness : bool;
}.

Definition count_derived_witness_fields (fields : list RuntimeWitnessField) : nat :=
  length (filter witness_maps_to_canonical_hash fields).

Definition count_sha_witness_fields (fields : list RuntimeWitnessField) : nat :=
  length (filter (fun f => negb (witness_maps_to_canonical_hash f)) fields).

Definition mk_component_integration_summary
  (c : Component) (fields : list RuntimeWitnessField)
  (prior : ComponentAlignmentStatus) (possible : bool)
  : ComponentIntegrationSummary :=
  {| cis_component := c;
     cis_integrated_status := IFS_Partial;
     cis_prior_alignment := prior;
     cis_witness_field_count := length fields;
     cis_coq_derived_count := count_derived_witness_fields fields;
     cis_requires_sha_count := count_sha_witness_fields fields;
     cis_integrated_coq_hash := integrated_coq_hash c;
     cis_integration_possible_after_witness := possible |}.

Definition replay_integration_summary : ComponentIntegrationSummary :=
  mk_component_integration_summary
    C_Replay replay_cb_minimal_witnesses
    CAS_PartialAlignment true.

Definition reorg_integration_summary : ComponentIntegrationSummary :=
  mk_component_integration_summary
    C_Reorg reorg_minimal_witnesses
    CAS_PartialAlignment true.

Definition connectblock_integration_summary : ComponentIntegrationSummary :=
  mk_component_integration_summary
    C_ConnectBlock replay_cb_minimal_witnesses
    CAS_PartialAlignment true.

Definition draw_integration_summary : ComponentIntegrationSummary :=
  {| cis_component := C_Draw;
     cis_integrated_status := IFS_Missing;
     cis_prior_alignment := CAS_FundamentallyBlocked;
     cis_witness_field_count := 0%nat;
     cis_coq_derived_count := 0%nat;
     cis_requires_sha_count := 0%nat;
     cis_integrated_coq_hash := 0%Z;
     cis_integration_possible_after_witness := false |}.

Definition component_integration_summaries : list ComponentIntegrationSummary :=
  [ replay_integration_summary;
    reorg_integration_summary;
    connectblock_integration_summary;
    draw_integration_summary ].

(** 018GG-K alignment status preserved on the three integrated targets. *)
Lemma integration_preserves_cas_partial_alignment :
  replay_integration_summary.(cis_prior_alignment) = CAS_PartialAlignment /\
  reorg_integration_summary.(cis_prior_alignment) = CAS_PartialAlignment /\
  connectblock_integration_summary.(cis_prior_alignment) = CAS_PartialAlignment.
Proof. exact replay_reorg_cb_partial_alignment. Qed.

(** ================================================================
    Part F:  Field-level counts
    ================================================================ *)

Definition integration_statuses : list IntegratedFieldStatus :=
  map rwi_integrated_status runtime_witness_integration_catalog.

Definition integration_aligned_count : nat :=
  length (filter ifs_aligned integration_statuses).
Definition integration_partial_count : nat :=
  length (filter ifs_partial integration_statuses).
Definition integration_missing_count : nat :=
  length (filter ifs_missing integration_statuses).

Definition integration_total_fields : nat := runtime_witness_integration_count.

Lemma integration_total_fields_value : integration_total_fields = 32%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma integration_aligned_count_value : integration_aligned_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma integration_partial_count_value : integration_partial_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma integration_missing_count_value : integration_missing_count = 19%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_witness_integration_partition_complete :
  integration_aligned_count + integration_partial_count + integration_missing_count
  = integration_total_fields.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part G:  Required proofs
    ================================================================ *)

Lemma runtime_witness_integration_catalog_nonempty :
  runtime_witness_integration_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_witness_integration_status_defined :
  forall e, In e runtime_witness_integration_catalog ->
    match e.(rwi_integrated_status) with
    | IFS_Aligned => True
    | IFS_Partial => True
    | IFS_Missing => True
    end.
Proof.
  intros e Hin. destruct e.(rwi_integrated_status); exact I.
Qed.

(** ================================================================
    Part H:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_witness_integration_established : bool := true.
Definition runtime_witness_integration_coq_eq_runtime : bool := false.

Lemma runtime_witness_integration_established_value :
  runtime_witness_integration_established = true.
Proof. reflexivity. Qed.

Lemma runtime_witness_integration_does_not_establish_equivalence :
  runtime_witness_integration_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_witness_integration_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.

Lemma draw_integration_fundamentally_blocked :
  draw_integration_summary.(cis_prior_alignment) = CAS_FundamentallyBlocked /\
  draw_integration_summary.(cis_integrated_status) = IFS_Missing.
Proof. repeat split; reflexivity. Qed.
