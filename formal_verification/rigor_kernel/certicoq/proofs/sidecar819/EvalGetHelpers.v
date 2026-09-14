From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors AST.
From compcert Require Selection.

Definition helpers_ok : bool :=
  match cminor_from_clight with
  | OK p =>
      match Selection.get_helpers (prog_defmap p) with
      | OK _ => true
      | Error _ => false
      end
  | Error _ => false
  end.

Timeout 120 Eval vm_compute in helpers_ok.
