(** TASK-018: CompCert backend passes after Cminor, no Compiler.v.

    Selection.sel_program uses [compile_switch] / [if_conversion_heuristic]
    Parameters only on some constructs.
    RTLgen.transl_program uses [more_likely] on every [Sifthenelse].
*)
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors RTL.
From compcert Require Selection RTLgen CminorSel.

Definition selection_res : res CminorSel.program :=
  match cminor_from_clight with
  | OK p => Selection.sel_program p
  | Error e => Error e
  end.

Definition selection_ok : bool :=
  match selection_res with
  | OK _ => true
  | Error _ => false
  end.

Definition rtl_res : res RTL.program :=
  match selection_res with
  | OK p => RTLgen.transl_program p
  | Error e => Error e
  end.

Definition rtl_ok : bool :=
  match rtl_res with
  | OK _ => true
  | Error _ => false
  end.
