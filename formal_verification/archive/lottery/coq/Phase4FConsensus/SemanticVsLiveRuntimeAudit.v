(** Phase4FConsensus/SemanticVsLiveRuntimeAudit.v
    Audit of Coq-semantic vs Live-runtime comparison (PHASE-4F-003P).

    Summary statistics over [semantic_vs_runtime_pairs]:
      agreement_count    = 0  (model heights/index != live 256-bit hashes/txids)
      disagreement_count = 4  (Draw / Replay / Reorg / ConnectBlock present both sides)
      missing_count      = 1  (Supply: no Coq-semantic observation)

    Disagreement is EXPECTED: it documents that the abstract Coq model payloads are
    not the live runtime hash values. This does NOT establish or refute Coq =
    Runtime (PD-4F-OB-02 / PD-4F-CPP-01 stay open). AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceCatalog.
Require Import Rigor.Phase4FConsensus.LiveRuntimeTraceIntegration.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeCatalog.

Definition agreement_count : nat :=
  semantic_trace_vs_runtime_agreement_count semantic_vs_runtime_pairs.
Definition disagreement_count : nat :=
  semantic_trace_vs_runtime_disagreement_count semantic_vs_runtime_pairs.
Definition missing_count : nat :=
  semantic_trace_vs_runtime_missing_count semantic_vs_runtime_pairs.

Lemma agreement_count_value : agreement_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma disagreement_count_value : disagreement_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma missing_count_value : missing_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** Conservation: every pair lands in exactly one bucket. *)
Lemma semantic_vs_runtime_total :
  agreement_count + disagreement_count + missing_count
    = length semantic_vs_runtime_pairs.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: catalog is non-empty. *)
Lemma catalog_nonempty : semantic_vs_runtime_pairs <> nil.
Proof. discriminate. Qed.

(** REQUIRED-3: at least one (Coq-semantic, live runtime) comparison pair exists,
    with the semantic side carrying Coq-semantic provenance (origin = 4). *)
Lemma semantic_vs_runtime_entries_exist :
  exists p, In p semantic_vs_runtime_pairs /\ (fst p).(so_origin) = 4%nat.
Proof.
  exists (sem_draw, project_live_runtime_to_coq_trace ev_003_winner). split.
  - simpl. left. reflexivity.
  - reflexivity.
Qed.
