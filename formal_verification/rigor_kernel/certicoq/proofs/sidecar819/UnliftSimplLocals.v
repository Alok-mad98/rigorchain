(** TASK 3.1B Phase I: SimplLocals on normalized Clight.

    Computational result only. Theorems that unfold
    [SimplLocals.transf_*] carry [Compopts.debug] — not Closed.
*)
From CheckOruGen Require Import check_oru_clight.
From Phase10 Require Import ClosedCompCertOru UnliftDef.
From compcert Require Import Errors AST Ctypes Clight.
From compcert Require SimplLocals.
From Coq Require Import List.

Definition sl_unlift_fun (f: function) : bool :=
  match SimplLocals.transf_function (unlift_function f) with
  | OK _ => true
  | Error _ => false
  end.

Definition sl_unlift_core_fun (f: function) : bool :=
  match SimplLocals.transf_function (unlift_function_core f) with
  | OK _ => true
  | Error _ => false
  end.

Fixpoint sl_all
  (g: function -> bool)
  (defs: list (ident * AST.globdef Clight.fundef type)) : bool :=
  match defs with
  | nil => true
  | (_, Gfun (Internal f)) :: rest =>
      if g f then sl_all g rest else false
  | _ :: rest => sl_all g rest
  end.

Definition sl_unlift_f_case_118 : bool := sl_unlift_fun f_f_case_118.
Definition sl_unlift_core_f_case_118 : bool := sl_unlift_core_fun f_f_case_118.
Definition sl_unlift_all_ok : bool := sl_all sl_unlift_fun (AST.prog_defs clight_prog).
Definition sl_unlift_core_all_ok : bool :=
  sl_all sl_unlift_core_fun (AST.prog_defs clight_prog).
