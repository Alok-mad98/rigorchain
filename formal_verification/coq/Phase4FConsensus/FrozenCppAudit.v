(** Phase4FConsensus/FrozenCppAudit.v
    Frozen C++ oracle presence audit (PHASE-4F-001J).

    Counts frozen C++ observations and confirms present entries exist. This is
    presence accounting ONLY: NO comparison against the Coq trace and NO
    equivalence claim (that is PD-4F-CPP-01, OPEN). AUDIT only. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.
Require Import Rigor.Phase4FConsensus.FrozenCppCatalog.

Definition frozen_cpp_count : nat := length frozen_cpp_catalog.

Definition frozen_cpp_present_count : nat :=
  length (filter fco_present frozen_cpp_catalog).

Lemma frozen_cpp_count_value : frozen_cpp_count = 5%nat.
Proof. reflexivity. Qed.

Lemma frozen_cpp_present_count_value : frozen_cpp_present_count = 4%nat.
Proof. reflexivity. Qed.

(** Catalog is non-empty. *)
Lemma catalog_nonempty : frozen_cpp_count <> 0%nat.
Proof. unfold frozen_cpp_count. simpl. discriminate. Qed.

(** At least one present frozen C++ observation exists. *)
Lemma catalog_present_entries_exist :
  exists o, In o frozen_cpp_catalog /\ fco_present o = true.
Proof.
  exists fco_draw. split.
  - simpl. left. reflexivity.
  - reflexivity.
Qed.
