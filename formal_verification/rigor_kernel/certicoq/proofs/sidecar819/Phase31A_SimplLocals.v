(** TASK 3.1A Phase A: exact SimplLocals [bad temporary] on check_oru.

    Does not modify the generated Clight. No Axiom / Admitted.
*)
From CheckOruGen Require Import check_oru_clight.
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors AST Ctypes Clight Cop.
From compcert Require SimplLocals.
From Coq Require Import List.
Import ListNotations.

Definition sl_fun (f: Clight.function) : res Clight.function :=
  SimplLocals.transf_function f.

Fixpoint first_sl_error
  (defs: list (ident * AST.globdef Clight.fundef type))
  : option (ident * errmsg) :=
  match defs with
  | nil => None
  | (id, Gfun (Internal f)) :: rest =>
      match sl_fun f with
      | Error e => Some (id, e)
      | OK _ => first_sl_error rest
      end
  | _ :: rest => first_sl_error rest
  end.

Definition first_fail : option (ident * errmsg) :=
  first_sl_error (AST.prog_defs clight_prog).

Definition first_fail_is_f_case_118 : bool :=
  match first_fail with
  | Some (id, _) =>
      match ident_eq id _f_case_118 with left _ => true | right _ => false end
  | None => false
  end.

Definition first_fail_temp_is_env_301 : bool :=
  match first_fail with
  | Some (_, MSG _ :: CTX tid :: _) =>
      match ident_eq tid __env_301 with left _ => true | right _ => false end
  | Some (_, _) => false
  | None => false
  end.

Definition env301_is_param_of_f_case_118 : bool :=
  match in_dec ident_eq __env_301 (Clight.var_names (fn_params f_f_case_118)) with
  | left _ => true
  | right _ => false
  end.

Definition env301_in_cenv : bool :=
  SimplLocals.VSet.mem __env_301 (SimplLocals.cenv_for f_f_case_118).

Definition env301_addr_taken : bool :=
  SimplLocals.VSet.mem __env_301 (SimplLocals.addr_taken_stmt (fn_body f_f_case_118)).

Definition sl_f_case_120_ok : bool :=
  match sl_fun f_f_case_120 with
  | OK _ => true
  | Error _ => false
  end.

Definition sl_f_case_119_ok : bool :=
  match sl_fun f_f_case_119 with
  | OK _ => true
  | Error _ => false
  end.

Definition sl_f_case_118_ok : bool :=
  match sl_fun f_f_case_118 with
  | OK _ => true
  | Error _ => false
  end.

Eval vm_compute in sl_f_case_120_ok.
Eval vm_compute in sl_f_case_119_ok.
Eval vm_compute in sl_f_case_118_ok.
Eval vm_compute in first_fail_is_f_case_118.
Eval vm_compute in first_fail_temp_is_env_301.
Eval vm_compute in env301_is_param_of_f_case_118.
Eval vm_compute in env301_in_cenv.
Eval vm_compute in env301_addr_taken.
