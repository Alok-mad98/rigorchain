(** Phase5Equivalence/ReorgEquivalenceCatalog.v
    Reorg equivalence observation catalog (TASK-018FF-D).

    Maps each Reorg observation pair (frozen=runtime identity, projection
    totality per sample reorg trace, semantic vs frozen, semantic vs runtime)
    into catalog rows with an honest [ReorgEquivResult]. No fabricated
    agreements; cross-domain disagreements are recorded explicitly. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase5Equivalence.ReorgEquivalence.

Record ReorgEquivCatalogEntry := {
  roec_kind : ReorgEquivKind;
  roec_trace_id : nat;
  roec_coq_height : Z;
  roec_frozen_payload : Z;
  roec_runtime_payload : Z;
  roec_present : bool;
  roec_result : ReorgEquivResult;
}.

(** Frozen-runtime identity row (TASK-005 fork-block hash). *)
Definition frozen_runtime_identity_row : ReorgEquivCatalogEntry :=
  {| roec_kind := RoEK_FrozenRuntimeIdentity;
     roec_trace_id := 0%nat;
     roec_coq_height := 0%Z;
     roec_frozen_payload := reorg_frozen_payload;
     roec_runtime_payload := reorg_runtime_payload;
     roec_present := true;
     roec_result :=
       if Z.eqb reorg_frozen_payload reorg_runtime_payload
       then RoER_Resolved else RoER_Disagreement |}.

(** Per-trace projection totality row. *)
Definition mk_totality_row (id : nat) (tr : consensus_trace)
  : ReorgEquivCatalogEntry :=
  {| roec_kind := RoEK_ProjectionTotality;
     roec_trace_id := id;
     roec_coq_height := coq_reorg_height tr;
     roec_frozen_payload := 0%Z;
     roec_runtime_payload := 0%Z;
     roec_present := true;
     roec_result := reorg_projection_status tr |}.

(** Per-trace semantic-vs-frozen row. *)
Definition mk_semantic_vs_frozen_row (id : nat) (tr : consensus_trace)
  : ReorgEquivCatalogEntry :=
  {| roec_kind := RoEK_SemanticVsFrozen;
     roec_trace_id := id;
     roec_coq_height := coq_reorg_height tr;
     roec_frozen_payload := reorg_frozen_payload;
     roec_runtime_payload := 0%Z;
     roec_present := true;
     roec_result := reorg_semantic_vs_frozen_result (coq_reorg_height tr) |}.

(** Per-trace semantic-vs-runtime row. *)
Definition mk_semantic_vs_runtime_row (id : nat) (tr : consensus_trace)
  : ReorgEquivCatalogEntry :=
  {| roec_kind := RoEK_SemanticVsRuntime;
     roec_trace_id := id;
     roec_coq_height := coq_reorg_height tr;
     roec_frozen_payload := 0%Z;
     roec_runtime_payload := reorg_runtime_payload;
     roec_present := true;
     roec_result := reorg_semantic_vs_runtime_result (coq_reorg_height tr) |}.

(** Build the per-kind sub-catalogs. [roec_trace_id]=0 is reserved for the
    frozen-runtime identity row. *)
Definition totality_catalog : list ReorgEquivCatalogEntry :=
  [ mk_totality_row 1 reorg_trace_one;
    mk_totality_row 2 reorg_trace_two;
    mk_totality_row 3 reorg_trace_three;
    mk_totality_row 4 reorg_trace_four ].

Definition semantic_vs_frozen_catalog : list ReorgEquivCatalogEntry :=
  [ mk_semantic_vs_frozen_row 1 reorg_trace_one;
    mk_semantic_vs_frozen_row 2 reorg_trace_two;
    mk_semantic_vs_frozen_row 3 reorg_trace_three;
    mk_semantic_vs_frozen_row 4 reorg_trace_four ].

Definition semantic_vs_runtime_catalog : list ReorgEquivCatalogEntry :=
  [ mk_semantic_vs_runtime_row 1 reorg_trace_one;
    mk_semantic_vs_runtime_row 2 reorg_trace_two;
    mk_semantic_vs_runtime_row 3 reorg_trace_three;
    mk_semantic_vs_runtime_row 4 reorg_trace_four ].

(** Full Reorg equivalence catalog. *)
Definition reorg_equivalence_catalog : list ReorgEquivCatalogEntry :=
  [ frozen_runtime_identity_row ] ++
  totality_catalog ++
  semantic_vs_frozen_catalog ++
  semantic_vs_runtime_catalog.

Definition reorg_catalog_count : nat := length reorg_equivalence_catalog.

Definition reorg_catalog_resolved_count : nat :=
  length (filter (fun r => reorg_result_resolved r.(roec_result))
                  reorg_equivalence_catalog).

Definition reorg_catalog_disagreement_count : nat :=
  length (filter (fun r => reorg_result_disagreement r.(roec_result))
                  reorg_equivalence_catalog).

Definition reorg_catalog_missing_count : nat :=
  length (filter (fun r => reorg_result_missing r.(roec_result))
                  reorg_equivalence_catalog).

Definition reorg_catalog_partial_count : nat :=
  length (filter (fun r => reorg_result_partial r.(roec_result))
                  reorg_equivalence_catalog).

Lemma reorg_equivalence_catalog_nonempty :
  reorg_equivalence_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma totality_catalog_count : length totality_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_frozen_catalog_count :
  length semantic_vs_frozen_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_runtime_catalog_count :
  length semantic_vs_runtime_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma reorg_catalog_count_value : reorg_catalog_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_catalog_resolved_count_value :
  reorg_catalog_resolved_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_catalog_disagreement_count_value :
  reorg_catalog_disagreement_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_catalog_missing_count_value :
  reorg_catalog_missing_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_catalog_partial_count_value :
  reorg_catalog_partial_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma reorg_catalog_counts_partition :
  reorg_catalog_resolved_count + reorg_catalog_disagreement_count +
  reorg_catalog_missing_count + reorg_catalog_partial_count
  = reorg_catalog_count.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_runtime_identity_row_resolved :
  frozen_runtime_identity_row.(roec_result) = RoER_Resolved.
Proof. vm_compute. reflexivity. Qed.

Lemma all_totality_rows_resolved :
  forall r, In r totality_catalog -> r.(roec_result) = RoER_Resolved.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_frozen_rows_disagree :
  forall r, In r semantic_vs_frozen_catalog ->
    r.(roec_result) = RoER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_runtime_rows_disagree :
  forall r, In r semantic_vs_runtime_catalog ->
    r.(roec_result) = RoER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.
