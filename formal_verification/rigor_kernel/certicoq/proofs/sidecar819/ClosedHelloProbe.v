(** TASK-018 control: tiny CertiCoq hello Clight through CompCert passes.
    Not a kernel theorem.
*)
From HelloGen Require Import prog.
From compcert Require Import Errors AST Clight Csharpminor Cminor CminorSel.
From compcert Require SimplLocals Cshmgen Cminorgen Selection RTLgen.

Definition hello_clight : Clight.program := prog.

Definition hello_sl_ok : bool :=
  match SimplLocals.transf_program hello_clight with
  | OK _ => true
  | Error _ => false
  end.

Definition hello_cshm_ok : bool :=
  match Cshmgen.transl_program hello_clight with
  | OK _ => true
  | Error _ => false
  end.

Definition hello_cminor : res Cminor.program :=
  match Cshmgen.transl_program hello_clight with
  | OK p => Cminorgen.transl_program p
  | Error e => Error e
  end.

Definition hello_cminor_ok : bool :=
  match hello_cminor with
  | OK _ => true
  | Error _ => false
  end.

Definition hello_sel : res CminorSel.program :=
  match hello_cminor with
  | OK p => Selection.sel_program p
  | Error e => Error e
  end.

Definition hello_sel_ok : bool :=
  match hello_sel with
  | OK _ => true
  | Error _ => false
  end.

Definition hello_rtl_ok : bool :=
  match hello_sel with
  | OK p =>
      match RTLgen.transl_program p with
      | OK _ => true
      | Error _ => false
      end
  | Error _ => false
  end.
