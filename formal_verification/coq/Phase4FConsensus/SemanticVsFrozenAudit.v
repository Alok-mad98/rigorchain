(** Phase4FConsensus/SemanticVsFrozenAudit.v
    Audit of Coq-semantic vs Frozen-C++ comparison (PHASE-4F-002O).

    Summary statistics over [semantic_vs_frozen_pairs]:
      agreement_count    = 0  (model heights/index != frozen 256-bit hashes)
      disagreement_count = 4  (Draw / Replay / Reorg / ConnectBlock present both sides)
      missing_count      = 1  (Supply: no Coq-semantic observation)

    Disagreement is EXPECTED: it documents that the abstract Coq model payloads are
    not the frozen C++ hash values. This does NOT establish or refute Coq = Frozen
    C++ (PD-4F-OB-02 / PD-4F-CPP-01 stay open). AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.SemanticObservationDerivation.
Require Import Rigor.Phase4FConsensus.SemanticObservationCatalog.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenCatalog.

Definition agreement_count : nat :=
  semantic_trace_agreement_count semantic_vs_frozen_pairs.
Definition disagreement_count : nat :=
  semantic_trace_disagreement_count semantic_vs_frozen_pairs.
Definition missing_count : nat :=
  semantic_trace_missing_count semantic_vs_frozen_pairs.

Lemma agreement_count_value : agreement_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma disagreement_count_value : disagreement_count = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma missing_count_value : missing_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** Conservation: every pair lands in exactly one bucket. *)
Lemma semantic_vs_frozen_total :
  agreement_count + disagreement_count + missing_count
    = length semantic_vs_frozen_pairs.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: catalog is non-empty. *)
Lemma catalog_nonempty : semantic_vs_frozen_pairs <> nil.
Proof. discriminate. Qed.

(** REQUIRED-3: at least one (Coq-semantic, frozen) comparison pair exists, with the
    semantic side carrying Coq-semantic provenance (origin = 4). *)
Lemma semantic_vs_frozen_entries_exist :
  exists p, In p semantic_vs_frozen_pairs /\ (fst p).(so_origin) = 4%nat.
Proof.
  exists (sem_draw, fco_draw). split.
  - simpl. left. reflexivity.
  - reflexivity.
Qed.
