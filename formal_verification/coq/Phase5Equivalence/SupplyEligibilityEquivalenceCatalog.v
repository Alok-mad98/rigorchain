(** Phase5Equivalence/SupplyEligibilityEquivalenceCatalog.v
    Supply + Eligibility/sigma equivalence catalog (TASK-018FF-F).

    Maps Supply missing-payload rows, per-trace supply projections, the typed
    [0 = 0] structural identity row, and per-draw-row eligibility identity rows
    into catalog entries with honest [SupplyEligEquivResult] values. *)

From Coq Require Import List ZArith Bool.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalence.

Record SupplyEligEquivCatalogEntry := {
  sece_kind : SupplyEligEquivKind;
  sece_row_id : nat;
  sece_coq_supply : Z;
  sece_frozen_present : bool;
  sece_frozen_payload : Z;
  sece_eligibility_flags : list bool;
  sece_imported_flags : list bool;
  sece_result : SupplyEligEquivResult;
}.

(** Supply: frozen side absent. *)
Definition supply_frozen_missing_row : SupplyEligEquivCatalogEntry :=
  {| sece_kind := SEEK_SupplyFrozenMissing;
     sece_row_id := 0%nat;
     sece_coq_supply := 0%Z;
     sece_frozen_present := frozen_supply_present;
     sece_frozen_payload := frozen_supply_payload_value;
     sece_eligibility_flags := nil;
     sece_imported_flags := nil;
     sece_result := supply_frozen_side_result |}.

(** Supply: runtime side absent (no single runtime supply payload). *)
Definition supply_runtime_missing_row : SupplyEligEquivCatalogEntry :=
  {| sece_kind := SEEK_SupplyRuntimeMissing;
     sece_row_id := 0%nat;
     sece_coq_supply := 0%Z;
     sece_frozen_present := false;
     sece_frozen_payload := 0%Z;
     sece_eligibility_flags := nil;
     sece_imported_flags := nil;
     sece_result := supply_runtime_side_result |}.

(** Supply: per-trace Coq projection row. *)
Definition mk_supply_projection_row (id : nat) (tr : consensus_trace)
  : SupplyEligEquivCatalogEntry :=
  {| sece_kind := SEEK_SupplyProjection;
     sece_row_id := id;
     sece_coq_supply := coq_supply_after_trace tr;
     sece_frozen_present := false;
     sece_frozen_payload := 0%Z;
     sece_eligibility_flags := nil;
     sece_imported_flags := nil;
     sece_result := supply_projection_row_result tr |}.

Definition supply_projection_catalog : list SupplyEligEquivCatalogEntry :=
  [ mk_supply_projection_row 1 supply_trace_empty;
    mk_supply_projection_row 2 supply_trace_one;
    mk_supply_projection_row 3 supply_trace_replay;
    mk_supply_projection_row 4 supply_trace_five ].

(** Supply: typed structural [0 = 0] identity (PARTIAL, not equivalence). *)
Definition supply_structural_identity_row : SupplyEligEquivCatalogEntry :=
  {| sece_kind := SEEK_SupplyStructuralIdentity;
     sece_row_id := 0%nat;
     sece_coq_supply := 0%Z;
     sece_frozen_present := frozen_supply_present;
     sece_frozen_payload := frozen_supply_payload_value;
     sece_eligibility_flags := nil;
     sece_imported_flags := nil;
     sece_result := supply_structural_identity_result |}.

(** Eligibility: per draw-audit row identity mapping. *)
Definition mk_eligibility_row (id : nat) (r : DrawAuditRow)
  : SupplyEligEquivCatalogEntry :=
  {| sece_kind := SEEK_EligibilityIdentity;
     sece_row_id := id;
     sece_coq_supply := 0%Z;
     sece_frozen_present := false;
     sece_frozen_payload := 0%Z;
     sece_eligibility_flags := coq_eligibility_flags (dar_candidates r);
     sece_imported_flags := map dc_eligible (dar_candidates r);
     sece_result := eligibility_row_result r |}.

Definition eligibility_identity_catalog : list SupplyEligEquivCatalogEntry :=
  map (fun r => mk_eligibility_row (dar_vector_id r) r) draw_audit_rows.

Definition se_equivalence_catalog : list SupplyEligEquivCatalogEntry :=
  [ supply_frozen_missing_row;
    supply_runtime_missing_row;
    supply_structural_identity_row ] ++
  supply_projection_catalog ++
  eligibility_identity_catalog.

Definition se_catalog_count : nat := length se_equivalence_catalog.

Definition se_catalog_resolved_count : nat :=
  length (filter (fun r => se_result_resolved r.(sece_result))
                  se_equivalence_catalog).

Definition se_catalog_disagreement_count : nat :=
  length (filter (fun r => se_result_disagreement r.(sece_result))
                  se_equivalence_catalog).

Definition se_catalog_missing_count : nat :=
  length (filter (fun r => se_result_missing r.(sece_result))
                  se_equivalence_catalog).

Definition se_catalog_partial_count : nat :=
  length (filter (fun r => se_result_partial r.(sece_result))
                  se_equivalence_catalog).

Lemma se_equivalence_catalog_nonempty :
  se_equivalence_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma supply_projection_catalog_count :
  length supply_projection_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma eligibility_identity_catalog_count :
  length eligibility_identity_catalog = 14%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_count_value : se_catalog_count = 21%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_resolved_count_value :
  se_catalog_resolved_count = 18%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_disagreement_count_value :
  se_catalog_disagreement_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_missing_count_value :
  se_catalog_missing_count = 2%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_partial_count_value :
  se_catalog_partial_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma se_catalog_counts_partition :
  se_catalog_resolved_count + se_catalog_disagreement_count +
  se_catalog_missing_count + se_catalog_partial_count
  = se_catalog_count.
Proof. vm_compute. reflexivity. Qed.

Lemma supply_frozen_missing_row_is_missing :
  supply_frozen_missing_row.(sece_result) = SEER_Missing.
Proof. exact supply_frozen_side_is_missing. Qed.

Lemma supply_runtime_missing_row_is_missing :
  supply_runtime_missing_row.(sece_result) = SEER_Missing.
Proof. reflexivity. Qed.

Lemma supply_structural_identity_row_is_partial :
  supply_structural_identity_row.(sece_result) = SEER_Partial.
Proof. exact supply_structural_identity_is_partial. Qed.

Lemma all_supply_projection_rows_resolved :
  forall r, In r supply_projection_catalog ->
    r.(sece_result) = SEER_Resolved.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_eligibility_rows_resolved :
  forall r, In r eligibility_identity_catalog ->
    r.(sece_result) = SEER_Resolved.
Proof.
  intros e Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst e; reflexivity | ]).
  contradiction.
Qed.
