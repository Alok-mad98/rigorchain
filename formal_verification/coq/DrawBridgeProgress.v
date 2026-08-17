(** DrawBridgeProgress.v — operator path for honesty greps.

    Canonical module: Phase5Equivalence/DrawBridgeProgress.v
    This file re-exports it so
      formal_verification/coq/DrawBridgeProgress.v
    contains the freeze flags.

    HONESTY: equivalence_claimed remains false.
    HONESTY: hash_kernel_shared_binary_with_cpp remains false. *)

From Rigor.Phase5Equivalence Require Export DrawBridgeProgress.

Lemma operator_path_equivalence_claimed : equivalence_claimed = false.
Proof. reflexivity. Qed.

Lemma operator_path_hash_kernel_not_shared :
  hash_kernel_shared_binary_with_cpp = false.
Proof. reflexivity. Qed.
