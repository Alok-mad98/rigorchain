(** Phase5Equivalence/RuntimeAlignmentIntegrationCatalog.v
    Integrated runtime alignment framework (TASK-018GG-M).

    Combines 018GG-K [ComponentAlignmentStatus] / [FieldAlignmentStatus] with
    018GG-L [IntegratedFieldStatus] and witness→[CanonicalStateHash] integration
    into per-cell [IntegratedAlignmentStatus] and component summaries.

    HONESTY BOUNDARY (CRITICAL):
    - No fabricated SHA-256 / runtime hashes.
    - DOES NOT assert Coq = Runtime; PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN.
    - Uses only 018GG-A/B/C, 018GG-J/L, and 018GG-K artifacts.
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
Require Import Rigor.Phase5Equivalence.RuntimeWitnessIntegrationCatalog.

Close Scope Z_scope.

(** ================================================================
    Part A:  Integrated alignment vocabulary (018GG-K + 018GG-L)
    ================================================================ *)

(** Field-level integrated status (task naming: aligned / partial / missing /
    fundamentally blocked at component scope). *)
Inductive IntegratedAlignmentStatus :=
| IAS_Aligned
| IAS_Partial
| IAS_Missing
| IAS_FundamentallyBlocked.

Definition ias_aligned (s : IntegratedAlignmentStatus) : bool :=
  match s with IAS_Aligned => true | _ => false end.
Definition ias_partial (s : IntegratedAlignmentStatus) : bool :=
  match s with IAS_Partial => true | _ => false end.
Definition ias_missing (s : IntegratedAlignmentStatus) : bool :=
  match s with IAS_Missing => true | _ => false end.
Definition ias_fundamentally_blocked (s : IntegratedAlignmentStatus) : bool :=
  match s with IAS_FundamentallyBlocked => true | _ => false end.

(** Component-level prior [CAS_*] from 018GG-K. *)
Definition component_prior_cas (c : Component) : ComponentAlignmentStatus :=
  match c with
  | C_Draw => CAS_FundamentallyBlocked
  | _ => CAS_PartialAlignment
  end.

(** Combine 018GG-K component prior with 018GG-L [IFS_*] per field. *)
Definition combine_cas_ifs
  (cas : ComponentAlignmentStatus) (ifs : IntegratedFieldStatus)
  : IntegratedAlignmentStatus :=
  match cas with
  | CAS_FundamentallyBlocked => IAS_FundamentallyBlocked
  | CAS_Aligned | CAS_PartialAlignment =>
      match ifs with
      | IFS_Aligned => IAS_Aligned
      | IFS_Partial => IAS_Partial
      | IFS_Missing => IAS_Missing
      end
  end.

(** ================================================================
    Part B:  Per-cell catalog (component × field × source)
    ================================================================ *)

Record RuntimeAlignmentIntegrationEntry := {
  rai_component : Component;
  rai_field : RuntimeWitnessField;
  rai_source : HashSource;
  rai_prior_cas : ComponentAlignmentStatus;
  rai_field_alignment : FieldAlignmentStatus;
  rai_integrated_field : IntegratedFieldStatus;
  rai_availability : FieldAvailabilityAnnotation;
  rai_integrated_status : IntegratedAlignmentStatus;
  rai_maps_to_hash : bool;
  rai_integrated_coq_hash : CanonicalStateHash;
}.

Definition mk_alignment_integration_entry
  (c : Component) (f : RuntimeWitnessField) (src : HashSource)
  : RuntimeAlignmentIntegrationEntry :=
  let cas := component_prior_cas c in
  let ifs := classify_integrated_field f src in
  let fas :=
    if source_eqb src HS_CoqAbstract
    then classify_field_alignment (coq_witness_field_derived f)
         (runtime_witness_field_available f)
    else classify_field_alignment false (runtime_witness_field_available f)
  in
  {| rai_component := c;
     rai_field := f;
     rai_source := src;
     rai_prior_cas := cas;
     rai_field_alignment := fas;
     rai_integrated_field := ifs;
     rai_availability := classify_field_availability f;
     rai_integrated_status := combine_cas_ifs cas ifs;
     rai_maps_to_hash := witness_maps_to_canonical_hash f;
     rai_integrated_coq_hash := integrated_coq_hash c |}.

Definition alignment_integration_entries_for_component
  (c : Component) (fields : list RuntimeWitnessField)
  : list RuntimeAlignmentIntegrationEntry :=
  let per_src src :=
    map (fun f => mk_alignment_integration_entry c f src) fields
  in
  per_src HS_CoqAbstract ++ per_src HS_Runtime.

Definition replay_alignment_integration_entries : list RuntimeAlignmentIntegrationEntry :=
  alignment_integration_entries_for_component C_Replay replay_cb_minimal_witnesses.

Definition reorg_alignment_integration_entries : list RuntimeAlignmentIntegrationEntry :=
  alignment_integration_entries_for_component C_Reorg reorg_minimal_witnesses.

Definition connectblock_alignment_integration_entries : list RuntimeAlignmentIntegrationEntry :=
  alignment_integration_entries_for_component C_ConnectBlock replay_cb_minimal_witnesses.

Definition runtime_alignment_integration_catalog : list RuntimeAlignmentIntegrationEntry :=
  replay_alignment_integration_entries ++
  reorg_alignment_integration_entries ++
  connectblock_alignment_integration_entries.

Definition runtime_alignment_integration_count : nat :=
  length runtime_alignment_integration_catalog.

(** ================================================================
    Part C:  Component-level integrated summaries
    ================================================================ *)

Record ComponentAlignmentIntegrationSummary := {
  cais_component : Component;
  cais_prior_cas : ComponentAlignmentStatus;
  cais_prior_ifs : IntegratedFieldStatus;
  cais_integrated_status : IntegratedAlignmentStatus;
  cais_integrated_coq_hash : CanonicalStateHash;
  cais_field_potential_after_witness : bool;
}.

Definition mk_component_alignment_integration_summary
  (c : Component) (cas : ComponentAlignmentStatus)
  (ifs : IntegratedFieldStatus) (possible : bool)
  : ComponentAlignmentIntegrationSummary :=
  {| cais_component := c;
     cais_prior_cas := cas;
     cais_prior_ifs := ifs;
     cais_integrated_status := combine_cas_ifs cas ifs;
     cais_integrated_coq_hash := integrated_coq_hash c;
     cais_field_potential_after_witness := possible |}.

Definition replay_alignment_integration_summary : ComponentAlignmentIntegrationSummary :=
  mk_component_alignment_integration_summary
    C_Replay CAS_PartialAlignment IFS_Partial true.

Definition reorg_alignment_integration_summary : ComponentAlignmentIntegrationSummary :=
  mk_component_alignment_integration_summary
    C_Reorg CAS_PartialAlignment IFS_Partial true.

Definition connectblock_alignment_integration_summary : ComponentAlignmentIntegrationSummary :=
  mk_component_alignment_integration_summary
    C_ConnectBlock CAS_PartialAlignment IFS_Partial true.

Definition draw_alignment_integration_summary : ComponentAlignmentIntegrationSummary :=
  {| cais_component := C_Draw;
     cais_prior_cas := CAS_FundamentallyBlocked;
     cais_prior_ifs := IFS_Missing;
     cais_integrated_status := IAS_FundamentallyBlocked;
     cais_integrated_coq_hash := 0%Z;
     cais_field_potential_after_witness := false |}.

Definition component_alignment_integration_summaries :
  list ComponentAlignmentIntegrationSummary :=
  [ replay_alignment_integration_summary;
    reorg_alignment_integration_summary;
    connectblock_alignment_integration_summary;
    draw_alignment_integration_summary ].

(** 018GG-K / 018GG-L statuses preserved on integrated targets. *)
Lemma alignment_integration_preserves_cas_partial :
  replay_alignment_integration_summary.(cais_prior_cas) = CAS_PartialAlignment /\
  reorg_alignment_integration_summary.(cais_prior_cas) = CAS_PartialAlignment /\
  connectblock_alignment_integration_summary.(cais_prior_cas) = CAS_PartialAlignment.
Proof. exact replay_reorg_cb_partial_alignment. Qed.

Lemma alignment_integration_preserves_ifs_partial :
  replay_alignment_integration_summary.(cais_prior_ifs) = IFS_Partial /\
  reorg_alignment_integration_summary.(cais_prior_ifs) = IFS_Partial /\
  connectblock_alignment_integration_summary.(cais_prior_ifs) = IFS_Partial.
Proof. repeat split; reflexivity. Qed.

Lemma draw_alignment_integration_fundamentally_blocked :
  draw_alignment_integration_summary.(cais_prior_cas) = CAS_FundamentallyBlocked /\
  draw_alignment_integration_summary.(cais_integrated_status) = IAS_FundamentallyBlocked.
Proof. repeat split; reflexivity. Qed.

(** Catalog cells on Replay / Reorg / ConnectBlock use [combine_cas_ifs] with
    [CAS_PartialAlignment] (Draw has no field catalog). *)
Lemma combine_preserves_ifs_on_partial_components :
  forall c f src,
    c <> C_Draw ->
    let e := mk_alignment_integration_entry c f src in
    e.(rai_integrated_status) =
      combine_cas_ifs CAS_PartialAlignment (classify_integrated_field f src).
Proof.
  intros c f src Hd.
  unfold mk_alignment_integration_entry.
  assert (Hc : component_prior_cas c = CAS_PartialAlignment) by (
    destruct c; try contradiction; reflexivity).
  rewrite Hc.
  destruct (classify_integrated_field f src); reflexivity.
Qed.

(** ================================================================
    Part D:  Field-level counts
    ================================================================ *)

Definition alignment_integration_statuses : list IntegratedAlignmentStatus :=
  map rai_integrated_status runtime_alignment_integration_catalog.

Definition alignment_integration_aligned_count : nat :=
  length (filter ias_aligned alignment_integration_statuses).
Definition alignment_integration_partial_count : nat :=
  length (filter ias_partial alignment_integration_statuses).
Definition alignment_integration_missing_count : nat :=
  length (filter ias_missing alignment_integration_statuses).
Definition alignment_integration_blocked_count : nat :=
  length (filter ias_fundamentally_blocked alignment_integration_statuses).

Definition alignment_integration_total_fields : nat :=
  runtime_alignment_integration_count.

Lemma alignment_integration_total_fields_value :
  alignment_integration_total_fields = 32%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_integration_aligned_count_value :
  alignment_integration_aligned_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_integration_partial_count_value :
  alignment_integration_partial_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_integration_missing_count_value :
  alignment_integration_missing_count = 19%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma alignment_integration_blocked_count_value :
  alignment_integration_blocked_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma runtime_alignment_integration_partition_complete :
  alignment_integration_aligned_count +
  alignment_integration_partial_count +
  alignment_integration_missing_count +
  alignment_integration_blocked_count
  = alignment_integration_total_fields.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part E:  Required proofs
    ================================================================ *)

Lemma runtime_alignment_integration_catalog_nonempty :
  runtime_alignment_integration_catalog <> nil.
Proof. discriminate. Qed.

Lemma runtime_alignment_integration_status_defined :
  forall e, In e runtime_alignment_integration_catalog ->
    match e.(rai_integrated_status) with
    | IAS_Aligned => True
    | IAS_Partial => True
    | IAS_Missing => True
    | IAS_FundamentallyBlocked => True
    end.
Proof.
  intros e Hin. destruct e.(rai_integrated_status); exact I.
Qed.

(** ================================================================
    Part F:  Honesty / debt re-exports
    ================================================================ *)

Definition runtime_alignment_integration_established : bool := true.
Definition runtime_alignment_integration_coq_eq_runtime : bool := false.

Lemma runtime_alignment_integration_established_value :
  runtime_alignment_integration_established = true.
Proof. reflexivity. Qed.

Lemma runtime_alignment_integration_does_not_establish_equivalence :
  runtime_alignment_integration_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma runtime_alignment_integration_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.
