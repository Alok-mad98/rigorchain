(** TASK-018: first computable slice of Selection on [check_oru] Cminor.

    [Selection.get_helpers] is Gallina and computes. [sel_program]
    after that did not finish in 180s (scale and/or
    [if_conversion_heuristic] Parameter).
*)
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors AST.
From compcert Require Selection.

Definition selection_helpers_ok : bool :=
  match cminor_from_clight with
  | OK p =>
      match Selection.get_helpers (prog_defmap p) with
      | OK _ => true
      | Error _ => false
      end
  | Error _ => false
  end.

Theorem selection_helpers_check_oru :
  selection_helpers_ok = true.
Proof.
  vm_compute.
  reflexivity.
Qed.
