(** TASK 3.1A Phase B: unlift experiment on check_oru.

    Original Clight is not modified. This file defines a separate
    [unlift_function] that rewrites [Sset] / [Etempvar] on
    [fn_params] into [Sassign (Evar _)] / [Evar], and prepends
    param-to-memory copies (the same shape as
    [SimplLocals.store_params] for non-lifted params).

    This is an experiment, not a claim that the official pipeline
    was run. Semantic preservation is NOT proved here.
*)
From CheckOruGen Require Import check_oru_clight.
From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Errors AST Ctypes Clight Cop.
From compcert Require SimplLocals.
From Coq Require Import List.

Definition param_type (ps: list (ident * type)) (id: ident) : option type :=
  match List.find
          (fun ity =>
             match ident_eq (fst ity) id with
             | left _ => true
             | right _ => false
             end)
          ps with
  | Some (_, ty) => Some ty
  | None => None
  end.

Fixpoint unlift_expr (ps: list (ident * type)) (a: expr) : expr :=
  match a with
  | Etempvar id ty =>
      match param_type ps id with
      | Some ty' => Evar id ty'
      | None => Etempvar id ty
      end
  | Ederef a1 ty => Ederef (unlift_expr ps a1) ty
  | Eaddrof a1 ty => Eaddrof (unlift_expr ps a1) ty
  | Eunop op a1 ty => Eunop op (unlift_expr ps a1) ty
  | Ebinop op a1 a2 ty => Ebinop op (unlift_expr ps a1) (unlift_expr ps a2) ty
  | Ecast a1 ty => Ecast (unlift_expr ps a1) ty
  | Efield a1 fld ty => Efield (unlift_expr ps a1) fld ty
  | Evar _ _ | Econst_int _ _ | Econst_float _ _ | Econst_single _ _
  | Econst_long _ _ | Esizeof _ _ | Ealignof _ _ => a
  end.

Fixpoint unlift_stmt (ps: list (ident * type)) (s: statement) : statement :=
  match s with
  | Sset id a =>
      match param_type ps id with
      | Some ty => Sassign (Evar id ty) (unlift_expr ps a)
      | None => Sset id (unlift_expr ps a)
      end
  | Sassign a1 a2 => Sassign (unlift_expr ps a1) (unlift_expr ps a2)
  | Scall oid a al =>
      Scall oid (unlift_expr ps a) (List.map (unlift_expr ps) al)
  | Sbuiltin oid ef tys al =>
      Sbuiltin oid ef tys (List.map (unlift_expr ps) al)
  | Ssequence s1 s2 => Ssequence (unlift_stmt ps s1) (unlift_stmt ps s2)
  | Sifthenelse a s1 s2 =>
      Sifthenelse (unlift_expr ps a) (unlift_stmt ps s1) (unlift_stmt ps s2)
  | Sloop s1 s2 => Sloop (unlift_stmt ps s1) (unlift_stmt ps s2)
  | Sreturn (Some a) => Sreturn (Some (unlift_expr ps a))
  | Sswitch a ls => Sswitch (unlift_expr ps a) (unlift_lblstmt ps ls)
  | Slabel lbl s1 => Slabel lbl (unlift_stmt ps s1)
  | Sskip | Sbreak | Scontinue | Sreturn None | Sgoto _ => s
  end
with unlift_lblstmt (ps: list (ident * type)) (ls: labeled_statements)
  : labeled_statements :=
  match ls with
  | LSnil => LSnil
  | LScons c s ls1 => LScons c (unlift_stmt ps s) (unlift_lblstmt ps ls1)
  end.

Fixpoint store_params (ps: list (ident * type)) (s: statement) : statement :=
  match ps with
  | nil => s
  | (id, ty) :: rest =>
      Ssequence (Sassign (Evar id ty) (Etempvar id ty)) (store_params rest s)
  end.

Definition unlift_function (f: function) : function :=
  {| fn_return := f.(fn_return);
     fn_callconv := f.(fn_callconv);
     fn_params := f.(fn_params);
     fn_vars := f.(fn_vars);
     fn_temps := f.(fn_temps);
     fn_body := store_params f.(fn_params)
                  (unlift_stmt f.(fn_params) f.(fn_body)) |}.

Definition sl_unlift_f_case_118_ok : bool :=
  match SimplLocals.transf_function (unlift_function f_f_case_118) with
  | OK _ => true
  | Error _ => false
  end.

Definition sl_unlift_f_case_120_ok : bool :=
  match SimplLocals.transf_function (unlift_function f_f_case_120) with
  | OK _ => true
  | Error _ => false
  end.

Fixpoint sl_unlift_all
  (defs: list (ident * AST.globdef Clight.fundef type)) : bool :=
  match defs with
  | nil => true
  | (_, Gfun (Internal f)) :: rest =>
      match SimplLocals.transf_function (unlift_function f) with
      | OK _ => sl_unlift_all rest
      | Error _ => false
      end
  | _ :: rest => sl_unlift_all rest
  end.

Definition sl_unlift_all_ok : bool :=
  sl_unlift_all (AST.prog_defs clight_prog).

(** Structural: [unlift_stmt] only turns [Sset] into [Sassign]
    when the id is a parameter. Not a Clight simulation. *)
Lemma unlift_sset_only_params :
  forall ps id a,
    match param_type ps id with
    | Some ty =>
        unlift_stmt ps (Sset id a) =
        Sassign (Evar id ty) (unlift_expr ps a)
    | None =>
        unlift_stmt ps (Sset id a) = Sset id (unlift_expr ps a)
    end.
Proof.
  intros ps id a.
  unfold unlift_stmt; fold unlift_stmt; fold unlift_expr; fold unlift_lblstmt.
  destruct (param_type ps id); reflexivity.
Qed.
