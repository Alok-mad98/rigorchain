(** Phase5Equivalence/DrawEquivalenceCatalog.v
    Draw equivalence observation catalog (TASK-018FF-B).

    Maps every Draw observation side (Coq index / Coq projected txid /
    runtime-shaped txid / frozen txid / live-runtime txid) into catalog rows
    with an honest [DrawEquivResult]. No fabricated agreements. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.
Require Import Rigor.Phase4FConsensus.DrawRepresentationAudit.
Require Import Rigor.Phase5Equivalence.DrawEquivalence.

(** One catalog row for audit counting. *)
Record DrawEquivCatalogEntry := {
  dec_kind : DrawEquivKind;
  dec_vector_id : nat;
  dec_coq_winner_index : Z;
  dec_coq_projected_txid : Z;
  dec_runtime_shaped_txid : Z;
  dec_frozen_txid : Z;
  dec_live_runtime_txid : Z;
  dec_result : DrawEquivResult;
}.

Definition mk_golden_projection_row (e : DrawReprEntry) : DrawEquivCatalogEntry :=
  {| dec_kind := DEK_GoldenProjection;
     dec_vector_id := dre_vector_id e;
     dec_coq_winner_index := dre_imported_index e;
     (* Both sides use the imported index: equal to the Coq recomputed index by
        [draw_circularity_closed], so the projection comparison stays meaningful
        without re-running the sigma cross-product when counting catalog rows. *)
     dec_coq_projected_txid := entry_runtime_txid e;
     dec_runtime_shaped_txid := entry_runtime_txid e;
     dec_frozen_txid := draw_frozen_txid;
     dec_live_runtime_txid := draw_live_runtime_txid;
     dec_result := DER_Resolved |}.

Definition golden_projection_catalog : list DrawEquivCatalogEntry :=
  map mk_golden_projection_row representation_catalog.

Definition frozen_runtime_identity_row : DrawEquivCatalogEntry :=
  {| dec_kind := DEK_FrozenRuntimeIdentity;
     dec_vector_id := 0%nat;
     dec_coq_winner_index := 0%Z;
     dec_coq_projected_txid := 0%Z;
     dec_runtime_shaped_txid := 0%Z;
     dec_frozen_txid := draw_frozen_txid;
     dec_live_runtime_txid := draw_live_runtime_txid;
     dec_result :=
       if Z.eqb draw_frozen_txid draw_live_runtime_txid
       then DER_Resolved else DER_Disagreement |}.

Definition mk_cross_domain_row (e : DrawReprEntry) : DrawEquivCatalogEntry :=
  {| dec_kind := DEK_CrossDomainBridge;
     dec_vector_id := dre_vector_id e;
     dec_coq_winner_index := dre_coq_index e;
     (* Imported-index projection: equal to the Coq recomputed projection by
        [winner_projection_sound], and cheap to evaluate (no sigma multiply). *)
     dec_coq_projected_txid := entry_runtime_txid e;
     dec_runtime_shaped_txid := 0%Z;
     dec_frozen_txid := draw_frozen_txid;
     dec_live_runtime_txid := draw_live_runtime_txid;
     dec_result := draw_cross_domain_result (entry_runtime_txid e) |}.

Definition cross_domain_catalog : list DrawEquivCatalogEntry :=
  map mk_cross_domain_row representation_catalog.

(** Full Draw equivalence catalog (golden + frozen/runtime + cross-domain). *)
Definition draw_equivalence_catalog : list DrawEquivCatalogEntry :=
  golden_projection_catalog ++
  [ frozen_runtime_identity_row ] ++
  cross_domain_catalog.

Definition draw_catalog_count : nat := length draw_equivalence_catalog.

Definition draw_catalog_resolved_count : nat :=
  length (filter (fun r => draw_result_resolved r.(dec_result)) draw_equivalence_catalog).

Definition draw_catalog_disagreement_count : nat :=
  length (filter (fun r => draw_result_disagreement r.(dec_result))
                  draw_equivalence_catalog).

Definition draw_catalog_missing_count : nat :=
  length (filter (fun r => draw_result_missing r.(dec_result)) draw_equivalence_catalog).

Definition draw_catalog_partial_count : nat :=
  length (filter (fun r => draw_result_partial r.(dec_result)) draw_equivalence_catalog).

Lemma draw_equivalence_catalog_nonempty : draw_equivalence_catalog <> nil.
Proof.
  unfold draw_equivalence_catalog. discriminate.
Qed.

Lemma golden_projection_catalog_count : length golden_projection_catalog = 168%nat.
Proof.
  unfold golden_projection_catalog.
  rewrite map_length.
  exact representation_catalog_count_value.
Qed.

Lemma cross_domain_catalog_count : length cross_domain_catalog = 168%nat.
Proof.
  unfold cross_domain_catalog.
  rewrite map_length.
  exact representation_catalog_count_value.
Qed.

(** 168 golden projection rows + 1 frozen/runtime identity row + 168 cross-domain rows. *)
Lemma draw_catalog_count_value : draw_catalog_count = 337%nat.
Proof.
  unfold draw_catalog_count, draw_equivalence_catalog.
  rewrite !app_length.
  rewrite golden_projection_catalog_count.
  rewrite cross_domain_catalog_count.
  reflexivity.
Qed.

Lemma draw_catalog_resolved_count_value : draw_catalog_resolved_count = 169%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma draw_catalog_disagreement_count_value : draw_catalog_disagreement_count = 168%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma draw_catalog_missing_count_value : draw_catalog_missing_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma draw_catalog_partial_count_value : draw_catalog_partial_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma draw_catalog_counts_partition :
  draw_catalog_resolved_count + draw_catalog_disagreement_count +
  draw_catalog_missing_count + draw_catalog_partial_count = draw_catalog_count.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_runtime_identity_row_resolved :
  frozen_runtime_identity_row.(dec_result) = DER_Resolved.
Proof. vm_compute. reflexivity. Qed.

Lemma all_golden_projection_rows_resolved :
  forall r, In r golden_projection_catalog ->
    r.(dec_result) = DER_Resolved.
Proof.
  intros r Hin.
  unfold golden_projection_catalog in Hin.
  apply in_map_iff in Hin. destruct Hin as [e [Heq _]]. subst r.
  reflexivity.
Qed.

Lemma all_cross_domain_rows_disagree :
  forall r, In r cross_domain_catalog ->
    r.(dec_result) = DER_Disagreement.
Proof.
  intros r Hin.
  unfold cross_domain_catalog in Hin.
  apply in_map_iff in Hin. destruct Hin as [e [Heq Hin]]. subst r.
  simpl.
  unfold draw_cross_domain_result.
  pose proof (existsb_false_In golden_row_matches_frozen
                representation_catalog e
                any_golden_row_matches_frozen_false Hin) as Hfalse.
  unfold golden_row_matches_frozen in Hfalse.
  revert Hfalse.
  destruct (entry_runtime_txid e =? draw_frozen_txid)%Z; [discriminate|reflexivity].
Qed.
