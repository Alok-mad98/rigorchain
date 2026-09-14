(** TASK-018: Closed Clight → CompCert probe on the existing
    [check_oru] Clight AST.

    Bypasses [print_Clight] (Compiler.v Parameter) and does not
    [Require] [Compiler]. Uses the same Gallina passes as
    [transf_clight_program] after the print hook:

      SimplLocals.transf_program
      Cshmgen.transl_program
      Cminorgen.transl_program

    A [ccomp] exit code is not this probe. No Axiom / Admitted.
    Sidecar Coq 8.19 only.
*)
From CheckOruGen Require Import check_oru_clight.
From compcert Require Import Errors AST Ctypes Clight Csharpminor Cminor.
From compcert Require SimplLocals Cshmgen Cminorgen.

Definition clight_prog : Clight.program := prog.

Definition n_globals : nat := List.length (AST.prog_defs clight_prog).

(** First official pass after [print print_Clight]. Mentions
    [Compopts.debug] (Parameter). *)
Definition simpllocals_res : res Clight.program :=
  SimplLocals.transf_program clight_prog.

Definition simpllocals_ok : bool :=
  match simpllocals_res with
  | OK _ => true
  | Error _ => false
  end.

(** C#minor generation. No Compopts / print Parameters. May be
    applied to the original Clight (official order is after
    SimplLocals). *)
Definition cshm_from_clight : res Csharpminor.program :=
  Cshmgen.transl_program clight_prog.

Definition cshm_from_clight_ok : bool :=
  match cshm_from_clight with
  | OK _ => true
  | Error _ => false
  end.

Definition cshm_from_simpllocals : res Csharpminor.program :=
  match simpllocals_res with
  | OK p1 => Cshmgen.transl_program p1
  | Error e => Error e
  end.

Definition cminor_from_clight : res Cminor.program :=
  match cshm_from_clight with
  | OK p1 => Cminorgen.transl_program p1
  | Error e => Error e
  end.

Definition cminor_from_clight_ok : bool :=
  match cminor_from_clight with
  | OK _ => true
  | Error _ => false
  end.
