(** Phase5Equivalence/ReplayEquivalenceCatalog.v
    Replay equivalence observation catalog (TASK-018FF-C).

    Maps each Replay observation pair (frozen=runtime identity, projection
    totality per sample trace, semantic vs frozen, semantic vs runtime) into
    catalog rows with an honest [ReplayEquivResult]. No fabricated agreements;
    cross-domain disagreements are recorded explicitly. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase5Equivalence.ReplayEquivalence.

Record ReplayEquivCatalogEntry := {
  rec_kind : ReplayEquivKind;
  rec_trace_id : nat;
  rec_coq_height : Z;
  rec_frozen_payload : Z;
  rec_runtime_payload : Z;
  rec_present : bool;
  rec_result : ReplayEquivResult;
}.

(** Frozen-runtime identity row (TASK-005 multinode tip). *)
Definition frozen_runtime_identity_row : ReplayEquivCatalogEntry :=
  {| rec_kind := REK_FrozenRuntimeIdentity;
     rec_trace_id := 0%nat;
     rec_coq_height := 0%Z;
     rec_frozen_payload := replay_frozen_payload;
     rec_runtime_payload := replay_runtime_payload;
     rec_present := true;
     rec_result :=
       if Z.eqb replay_frozen_payload replay_runtime_payload
       then RER_Resolved else RER_Disagreement |}.

(** Per-trace projection totality row. *)
Definition mk_totality_row (id : nat) (tr : consensus_trace)
  : ReplayEquivCatalogEntry :=
  {| rec_kind := REK_ProjectionTotality;
     rec_trace_id := id;
     rec_coq_height := coq_replay_height tr;
     rec_frozen_payload := 0%Z;
     rec_runtime_payload := 0%Z;
     rec_present := true;
     rec_result := replay_projection_status tr |}.

(** Per-trace semantic-vs-frozen row. *)
Definition mk_semantic_vs_frozen_row (id : nat) (tr : consensus_trace)
  : ReplayEquivCatalogEntry :=
  {| rec_kind := REK_SemanticVsFrozen;
     rec_trace_id := id;
     rec_coq_height := coq_replay_height tr;
     rec_frozen_payload := replay_frozen_payload;
     rec_runtime_payload := 0%Z;
     rec_present := true;
     rec_result := replay_semantic_vs_frozen_result (coq_replay_height tr) |}.

(** Per-trace semantic-vs-runtime row. *)
Definition mk_semantic_vs_runtime_row (id : nat) (tr : consensus_trace)
  : ReplayEquivCatalogEntry :=
  {| rec_kind := REK_SemanticVsRuntime;
     rec_trace_id := id;
     rec_coq_height := coq_replay_height tr;
     rec_frozen_payload := 0%Z;
     rec_runtime_payload := replay_runtime_payload;
     rec_present := true;
     rec_result := replay_semantic_vs_runtime_result (coq_replay_height tr) |}.

(** Build the per-kind sub-catalogs. We index by 1..4 so [rec_trace_id]=0 is
    reserved for the frozen-runtime identity row. *)
Definition totality_catalog : list ReplayEquivCatalogEntry :=
  [ mk_totality_row 1 replay_trace_empty;
    mk_totality_row 2 replay_trace_one;
    mk_totality_row 3 replay_trace_three;
    mk_totality_row 4 replay_trace_five ].

Definition semantic_vs_frozen_catalog : list ReplayEquivCatalogEntry :=
  [ mk_semantic_vs_frozen_row 1 replay_trace_empty;
    mk_semantic_vs_frozen_row 2 replay_trace_one;
    mk_semantic_vs_frozen_row 3 replay_trace_three;
    mk_semantic_vs_frozen_row 4 replay_trace_five ].

Definition semantic_vs_runtime_catalog : list ReplayEquivCatalogEntry :=
  [ mk_semantic_vs_runtime_row 1 replay_trace_empty;
    mk_semantic_vs_runtime_row 2 replay_trace_one;
    mk_semantic_vs_runtime_row 3 replay_trace_three;
    mk_semantic_vs_runtime_row 4 replay_trace_five ].

(** Full Replay equivalence catalog. *)
Definition replay_equivalence_catalog : list ReplayEquivCatalogEntry :=
  [ frozen_runtime_identity_row ] ++
  totality_catalog ++
  semantic_vs_frozen_catalog ++
  semantic_vs_runtime_catalog.

Definition replay_catalog_count : nat := length replay_equivalence_catalog.

Definition replay_catalog_resolved_count : nat :=
  length (filter (fun r => replay_result_resolved r.(rec_result))
                  replay_equivalence_catalog).

Definition replay_catalog_disagreement_count : nat :=
  length (filter (fun r => replay_result_disagreement r.(rec_result))
                  replay_equivalence_catalog).

Definition replay_catalog_missing_count : nat :=
  length (filter (fun r => replay_result_missing r.(rec_result))
                  replay_equivalence_catalog).

Definition replay_catalog_partial_count : nat :=
  length (filter (fun r => replay_result_partial r.(rec_result))
                  replay_equivalence_catalog).

Lemma replay_equivalence_catalog_nonempty :
  replay_equivalence_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma totality_catalog_count : length totality_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_frozen_catalog_count :
  length semantic_vs_frozen_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_runtime_catalog_count :
  length semantic_vs_runtime_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma replay_catalog_count_value : replay_catalog_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_catalog_resolved_count_value :
  replay_catalog_resolved_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_catalog_disagreement_count_value :
  replay_catalog_disagreement_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_catalog_missing_count_value :
  replay_catalog_missing_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_catalog_partial_count_value :
  replay_catalog_partial_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma replay_catalog_counts_partition :
  replay_catalog_resolved_count + replay_catalog_disagreement_count +
  replay_catalog_missing_count + replay_catalog_partial_count
  = replay_catalog_count.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_runtime_identity_row_resolved :
  frozen_runtime_identity_row.(rec_result) = RER_Resolved.
Proof. vm_compute. reflexivity. Qed.

Lemma all_totality_rows_resolved :
  forall r, In r totality_catalog -> r.(rec_result) = RER_Resolved.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_frozen_rows_disagree :
  forall r, In r semantic_vs_frozen_catalog ->
    r.(rec_result) = RER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_runtime_rows_disagree :
  forall r, In r semantic_vs_runtime_catalog ->
    r.(rec_result) = RER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.
