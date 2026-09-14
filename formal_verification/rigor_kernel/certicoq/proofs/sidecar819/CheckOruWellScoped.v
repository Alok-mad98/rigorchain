(** [unique_bindings] on the CPS [exp] of [check_oru] did not
    reduce to [True] under [vm_compute] (residual [Fcons]/[Eapp]
    term, 2026-09-13). This file records that fact. It does not
    replay the dump. *)
Definition unique_bindings_reduced_to_true : bool := false.

Theorem unique_bindings_not_established :
  unique_bindings_reduced_to_true = false.
Proof. reflexivity. Qed.
