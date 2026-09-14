(** TASK 3.1A Phase C: exact Selection blocker on check_oru Cminor.

    Traces [sel_program] = [get_helpers] then [sel_fundef] /
    [sel_stmt]. Records whether the Cminor contains [Sswitch]
    (calls [compile_switch]) and whether [if_conversion_heuristic]
    is reachable.
*)
From Phase10 Require Import ClosedCompCertOru ClosedCompCertOruBackend.
From compcert Require Import Errors AST Cminor CminorSel.
From compcert Require Selection Cminortyping.
From Coq Require Import List.

Fixpoint stmt_has_switch (s: Cminor.stmt) : bool :=
  match s with
  | Cminor.Sswitch _ _ _ _ => true
  | Cminor.Sseq s1 s2 => stmt_has_switch s1 || stmt_has_switch s2
  | Cminor.Sifthenelse _ s1 s2 => stmt_has_switch s1 || stmt_has_switch s2
  | Cminor.Sloop s1 => stmt_has_switch s1
  | Cminor.Sblock s1 => stmt_has_switch s1
  | Cminor.Slabel _ s1 => stmt_has_switch s1
  | _ => false
  end.

Fixpoint stmt_has_if (s: Cminor.stmt) : bool :=
  match s with
  | Cminor.Sifthenelse _ _ _ => true
  | Cminor.Sseq s1 s2 => stmt_has_if s1 || stmt_has_if s2
  | Cminor.Sloop s1 => stmt_has_if s1
  | Cminor.Sblock s1 => stmt_has_if s1
  | Cminor.Slabel _ s1 => stmt_has_if s1
  | Cminor.Sswitch _ _ _ _ => false
  | _ => false
  end.

Definition fundef_has_switch (fd: Cminor.fundef) : bool :=
  match fd with
  | Internal f => stmt_has_switch f.(Cminor.fn_body)
  | External _ => false
  end.

Definition fundef_has_if (fd: Cminor.fundef) : bool :=
  match fd with
  | Internal f => stmt_has_if f.(Cminor.fn_body)
  | External _ => false
  end.

Definition cminor_has_switch : bool :=
  match cminor_from_clight with
  | OK p =>
      List.existsb
        (fun '(id, gd) =>
           match gd with
           | Gfun fd => fundef_has_switch fd
           | Gvar _ => false
           end)
        (AST.prog_defs p)
  | Error _ => false
  end.

Definition cminor_has_if : bool :=
  match cminor_from_clight with
  | OK p =>
      List.existsb
        (fun '(id, gd) =>
           match gd with
           | Gfun fd => fundef_has_if fd
           | Gvar _ => false
           end)
        (AST.prog_defs p)
  | Error _ => false
  end.

(** Helpers already Closed in TASK-018. *)
Definition helpers_ok : bool :=
  match cminor_from_clight with
  | OK p =>
      match Selection.get_helpers (prog_defmap p) with
      | OK _ => true
      | Error _ => false
      end
  | Error _ => false
  end.

Eval vm_compute in helpers_ok.
Eval vm_compute in cminor_has_switch.
Eval vm_compute in cminor_has_if.
