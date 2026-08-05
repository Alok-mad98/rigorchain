(** Phase4FConsensus/DrawRepresentationCatalog.v
    Catalog of draw representation entries (PHASE-4F-001I).

    The 14 draw-applicable rows, imported ONLY from existing Phase-4F structures
    (`ConsensusGoldenVectorsData.draw_audit_rows` + `ConsensusDrawAudit`). No JSON
    is re-read. Each entry carries the candidate txids, the Coq-recomputed winner
    index (001E), and the imported winner index. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenVectorsData.
Require Import Rigor.Phase4FConsensus.ConsensusDrawAudit.
Require Import Rigor.Phase4FConsensus.DrawRepresentationBridge.

Record DrawReprEntry := {
  dre_vector_id : nat;
  dre_candidates : list DrawCandidate;
  dre_coq_index : Z;        (* independently recomputed (001E) *)
  dre_imported_index : Z;   (* imported kernel/golden winner index *)
}.

(** REQUIRED-2: 14 draw vectors, all imported from existing structures. *)
Definition representation_catalog : list DrawReprEntry :=
  map (fun r =>
        {| dre_vector_id := dar_vector_id r;
           dre_candidates := dar_candidates r;
           dre_coq_index := IndependentWinnerSource r;
           dre_imported_index := dar_imported_winner r |})
      draw_audit_rows.

Definition representation_catalog_count : nat := length representation_catalog.

Lemma representation_catalog_count_value : representation_catalog_count = 14%nat.
Proof. reflexivity. Qed.

(** REQUIRED-4: catalog is non-empty. *)
Lemma representation_catalog_nonempty : representation_catalog_count <> 0%nat.
Proof. unfold representation_catalog_count. simpl. discriminate. Qed.
