(** Phase4FConsensus/FrozenCppAgreementAudit.v
    Frozen C++ agreement statistics (PHASE-4F-001K).

    Instantiates the comparison over the mapping catalog and proves the
    statistics. The agreements are payload equalities of IMPORTED values
    (frozen-vs-runtime cross-consistency); the single Draw disagreement shows the
    Coq trace domain is not identified with the live C++ output. This does NOT
    establish Coq = Frozen C++ (PD-4F-CPP-01 / PD-4F-OB-02). AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreementCatalog.

Definition frozen_agreement_count : nat := agreement_count frozen_agreement_pairs.
Definition frozen_disagreement_count : nat := disagreement_count frozen_agreement_pairs.
Definition frozen_missing_count : nat := missing_count frozen_agreement_pairs.

(** Honest statistics: 3 agree (runtime cross-consistency), 1 disagree (draw
    domain), 1 missing (supply). *)
Lemma frozen_agreement_count_value : frozen_agreement_count = 3%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_disagreement_count_value : frozen_disagreement_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma frozen_missing_count_value : frozen_missing_count = 1%nat.
Proof. vm_compute. reflexivity. Qed.

(** REQUIRED-3: mapping catalog is non-empty. *)
Lemma catalog_nonempty : frozen_agreement_pairs <> nil.
Proof. unfold frozen_agreement_pairs. discriminate. Qed.

(** REQUIRED-3: the agreement statistics are total (partition the catalog). *)
Lemma agreement_statistics_exist :
  (frozen_agreement_count + frozen_disagreement_count + frozen_missing_count)%nat
    = length frozen_agreement_pairs.
Proof. vm_compute. reflexivity. Qed.
