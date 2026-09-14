(** Phase4GRefinement/EquivalenceObligationAudit.v
    Count theorem obligations for TASK-010X.

    This file reports only counts over EquivalenceObligationCatalog.v. It does not
    introduce a readiness/comparison/oracle/bridge layer and does not prove
    equivalence. *)

From Coq Require Import List.
Import ListNotations.
Require Import Rigor.Phase4GRefinement.EquivalenceObligationCatalog.

Definition total_obligations : nat := length obligation_catalog.

Definition blocking_obligations : nat :=
  length (filter obligation_is_blocking obligation_catalog).

Definition resolved_obligations : nat :=
  length (filter (fun o => negb o.(eo_blocking)) obligation_catalog).

Lemma total_obligations_value : total_obligations = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma blocking_obligations_value : blocking_obligations = 4%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma resolved_obligations_value : resolved_obligations = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma obligation_audit_counts_partition :
  blocking_obligations + resolved_obligations = total_obligations.
Proof. vm_compute. reflexivity. Qed.

Lemma obligation_disagreement_total_value : accounted_disagreement_count = 8%nat.
Proof. exact all_disagreements_accounted_for. Qed.
