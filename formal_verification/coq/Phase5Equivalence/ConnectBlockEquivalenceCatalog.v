(** Phase5Equivalence/ConnectBlockEquivalenceCatalog.v
    ConnectBlock equivalence observation catalog (TASK-018FF-E).

    Maps each ConnectBlock observation pair (frozen=runtime identity,
    projection totality per sample trace, semantic vs frozen, semantic vs
    runtime) into catalog rows with an honest [ConnectBlockEquivResult]. No
    fabricated agreements; cross-domain disagreements are recorded explicitly. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusMachine.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalence.

Record ConnectBlockEquivCatalogEntry := {
  cbec_kind : ConnectBlockEquivKind;
  cbec_trace_id : nat;
  cbec_coq_height : Z;
  cbec_frozen_payload : Z;
  cbec_runtime_payload : Z;
  cbec_present : bool;
  cbec_result : ConnectBlockEquivResult;
}.

(** Frozen-runtime identity row (TASK-005 sample block hash). *)
Definition frozen_runtime_identity_row : ConnectBlockEquivCatalogEntry :=
  {| cbec_kind := CBEK_FrozenRuntimeIdentity;
     cbec_trace_id := 0%nat;
     cbec_coq_height := 0%Z;
     cbec_frozen_payload := cb_frozen_payload;
     cbec_runtime_payload := cb_runtime_payload;
     cbec_present := true;
     cbec_result :=
       if Z.eqb cb_frozen_payload cb_runtime_payload
       then CBER_Resolved else CBER_Disagreement |}.

(** Per-trace projection totality row. *)
Definition mk_totality_row (id : nat) (tr : consensus_trace)
  : ConnectBlockEquivCatalogEntry :=
  {| cbec_kind := CBEK_ProjectionTotality;
     cbec_trace_id := id;
     cbec_coq_height := coq_cb_height tr;
     cbec_frozen_payload := 0%Z;
     cbec_runtime_payload := 0%Z;
     cbec_present := true;
     cbec_result := cb_projection_status tr |}.

(** Per-trace semantic-vs-frozen row. *)
Definition mk_semantic_vs_frozen_row (id : nat) (tr : consensus_trace)
  : ConnectBlockEquivCatalogEntry :=
  {| cbec_kind := CBEK_SemanticVsFrozen;
     cbec_trace_id := id;
     cbec_coq_height := coq_cb_height tr;
     cbec_frozen_payload := cb_frozen_payload;
     cbec_runtime_payload := 0%Z;
     cbec_present := true;
     cbec_result := cb_semantic_vs_frozen_result (coq_cb_height tr) |}.

(** Per-trace semantic-vs-runtime row. *)
Definition mk_semantic_vs_runtime_row (id : nat) (tr : consensus_trace)
  : ConnectBlockEquivCatalogEntry :=
  {| cbec_kind := CBEK_SemanticVsRuntime;
     cbec_trace_id := id;
     cbec_coq_height := coq_cb_height tr;
     cbec_frozen_payload := 0%Z;
     cbec_runtime_payload := cb_runtime_payload;
     cbec_present := true;
     cbec_result := cb_semantic_vs_runtime_result (coq_cb_height tr) |}.

(** Build per-kind sub-catalogs. [cbec_trace_id]=0 reserved for the identity row. *)
Definition totality_catalog : list ConnectBlockEquivCatalogEntry :=
  [ mk_totality_row 1 cb_trace_phase4f;
    mk_totality_row 2 cb_trace_two;
    mk_totality_row 3 cb_trace_three;
    mk_totality_row 4 cb_trace_seven ].

Definition semantic_vs_frozen_catalog : list ConnectBlockEquivCatalogEntry :=
  [ mk_semantic_vs_frozen_row 1 cb_trace_phase4f;
    mk_semantic_vs_frozen_row 2 cb_trace_two;
    mk_semantic_vs_frozen_row 3 cb_trace_three;
    mk_semantic_vs_frozen_row 4 cb_trace_seven ].

Definition semantic_vs_runtime_catalog : list ConnectBlockEquivCatalogEntry :=
  [ mk_semantic_vs_runtime_row 1 cb_trace_phase4f;
    mk_semantic_vs_runtime_row 2 cb_trace_two;
    mk_semantic_vs_runtime_row 3 cb_trace_three;
    mk_semantic_vs_runtime_row 4 cb_trace_seven ].

(** Full ConnectBlock equivalence catalog. *)
Definition cb_equivalence_catalog : list ConnectBlockEquivCatalogEntry :=
  [ frozen_runtime_identity_row ] ++
  totality_catalog ++
  semantic_vs_frozen_catalog ++
  semantic_vs_runtime_catalog.

Definition cb_catalog_count : nat := length cb_equivalence_catalog.

Definition cb_catalog_resolved_count : nat :=
  length (filter (fun r => cb_result_resolved r.(cbec_result))
                  cb_equivalence_catalog).

Definition cb_catalog_disagreement_count : nat :=
  length (filter (fun r => cb_result_disagreement r.(cbec_result))
                  cb_equivalence_catalog).

Definition cb_catalog_missing_count : nat :=
  length (filter (fun r => cb_result_missing r.(cbec_result))
                  cb_equivalence_catalog).

Definition cb_catalog_partial_count : nat :=
  length (filter (fun r => cb_result_partial r.(cbec_result))
                  cb_equivalence_catalog).

Lemma cb_equivalence_catalog_nonempty :
  cb_equivalence_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma totality_catalog_count : length totality_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_frozen_catalog_count :
  length semantic_vs_frozen_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma semantic_vs_runtime_catalog_count :
  length semantic_vs_runtime_catalog = 4%nat.
Proof. reflexivity. Qed.

Lemma cb_catalog_count_value : cb_catalog_count = 13%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma cb_catalog_resolved_count_value :
  cb_catalog_resolved_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma cb_catalog_disagreement_count_value :
  cb_catalog_disagreement_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma cb_catalog_missing_count_value :
  cb_catalog_missing_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma cb_catalog_partial_count_value :
  cb_catalog_partial_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma cb_catalog_counts_partition :
  cb_catalog_resolved_count + cb_catalog_disagreement_count +
  cb_catalog_missing_count + cb_catalog_partial_count
  = cb_catalog_count.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_runtime_identity_row_resolved :
  frozen_runtime_identity_row.(cbec_result) = CBER_Resolved.
Proof. vm_compute. reflexivity. Qed.

Lemma all_totality_rows_resolved :
  forall r, In r totality_catalog -> r.(cbec_result) = CBER_Resolved.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_frozen_rows_disagree :
  forall r, In r semantic_vs_frozen_catalog ->
    r.(cbec_result) = CBER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.

Lemma all_semantic_vs_runtime_rows_disagree :
  forall r, In r semantic_vs_runtime_catalog ->
    r.(cbec_result) = CBER_Disagreement.
Proof.
  intros r Hin. revert Hin.
  vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst r; reflexivity | ]).
  contradiction.
Qed.
