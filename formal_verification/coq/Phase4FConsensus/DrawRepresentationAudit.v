(** Phase4FConsensus/DrawRepresentationAudit.v
    Draw representation closure audit (PHASE-4F-001I).

    For every draw entry, projects BOTH the Coq-recomputed winner index and the
    imported winner index into the txid domain and checks they denote the SAME
    txid object. This closes the index-vs-txid representation gap WITHIN the draw
    domain. It does NOT connect to the live runtime block (different scenario) or
    to C++; those remain in PD-4F-OB-01b / PD-4F-OB-02. AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusGoldenTrace.
Require Import Rigor.Phase4FConsensus.DrawRepresentationBridge.
Require Import Rigor.Phase4FConsensus.DrawRepresentationCatalog.

Definition entry_agreement (e : DrawReprEntry) : DrawRepresentationAgreement :=
  compare_representation e.(dre_candidates) e.(dre_coq_index) e.(dre_imported_index).

Definition entry_coq_txid (e : DrawReprEntry) : Z :=
  winner_index_to_txid e.(dre_candidates) e.(dre_coq_index).

Definition entry_runtime_txid (e : DrawReprEntry) : Z :=
  winner_txid_projection e.(dre_candidates) e.(dre_imported_index).

Definition representation_results : list DrawRepresentationAgreement :=
  map entry_agreement representation_catalog.

Definition is_repr_match (a : DrawRepresentationAgreement) : bool :=
  match a with DRA_Match => true | _ => false end.

Definition representation_match_count : nat :=
  length (filter is_repr_match representation_results).

Definition representation_mismatch_count : nat :=
  length (filter (fun a => negb (is_repr_match a)) representation_results).

(** REQUIRED-4: all 14 draw vectors close in the txid domain. *)
Lemma representation_match_count_value : representation_match_count = 14%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma representation_mismatch_count_value : representation_mismatch_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-4: the Coq-side winner txid equals the imported (runtime-shaped)
    winner txid for every catalog entry — the representation closure. *)
Theorem winner_projection_sound :
  forall e, In e representation_catalog ->
            entry_coq_txid e = entry_runtime_txid e.
Proof.
  intros e Hin. revert Hin. vm_compute.
  intros Hin.
  repeat (destruct Hin as [Heq | Hin]; [subst e; reflexivity | ]).
  contradiction.
Qed.
