(** TASK 3.1B: frozen [unlift] transformation.

    Input/output: [Clight.function]. Does not edit generated Clight
    or CompCert. Structural facts only in this file.
*)
From Coq Require Import List.
From compcert Require Import Coqlib AST Ctypes Cop Clight.

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

(** Body-only rewrite: inverse-of-lifting shape. Semantic candidate
    under [function_entry1] (params allocated in memory). *)
Definition unlift_body (f: function) : statement :=
  unlift_stmt f.(fn_params) f.(fn_body).

Definition unlift_function_core (f: function) : function :=
  {| fn_return := f.(fn_return);
     fn_callconv := f.(fn_callconv);
     fn_params := f.(fn_params);
     fn_vars := f.(fn_vars);
     fn_temps := f.(fn_temps);
     fn_body := unlift_body f |}.

(** Frozen 3.1A experiment: prepends [store_params]. Requires a
    hybrid entry that CompCert does not define (see UnliftRelation). *)
Definition unlift_function (f: function) : function :=
  {| fn_return := f.(fn_return);
     fn_callconv := f.(fn_callconv);
     fn_params := f.(fn_params);
     fn_vars := f.(fn_vars);
     fn_temps := f.(fn_temps);
     fn_body := store_params f.(fn_params) (unlift_body f) |}.

Definition unlift_fundef (fd: fundef) : fundef :=
  match fd with
  | Internal f => Internal (unlift_function f)
  | External _ _ _ _ => fd
  end.

(** [Etempvar id ty] rewritten to [Evar id ty'] uses the parameter
    declaration type [ty'], which may differ from the temp annotation
    [ty]. Other constructors keep the node's type. *)
Lemma unlift_typeof :
  forall ps a,
    match a with
    | Etempvar id ty =>
        match param_type ps id with
        | Some ty' => typeof (unlift_expr ps a) = ty'
        | None => typeof (unlift_expr ps a) = ty
        end
    | _ => typeof (unlift_expr ps a) = typeof a
    end.
Proof.
  destruct a; simpl; try reflexivity;
    destruct (param_type ps i); reflexivity.
Qed.

Lemma unlift_typeof_non_tempvar :
  forall ps a,
    (forall id ty, a <> Etempvar id ty) ->
    typeof (unlift_expr ps a) = typeof a.
Proof.
  intros ps a H.
  destruct a; try reflexivity.
  exfalso. apply (H i t). reflexivity.
Qed.

Lemma unlift_preserves_control_structure :
  forall ps s1 s2,
    unlift_stmt ps (Ssequence s1 s2) =
    Ssequence (unlift_stmt ps s1) (unlift_stmt ps s2).
Proof. reflexivity. Qed.

Lemma unlift_preserves_if_shape :
  forall ps a s1 s2,
    unlift_stmt ps (Sifthenelse a s1 s2) =
    Sifthenelse (unlift_expr ps a) (unlift_stmt ps s1) (unlift_stmt ps s2).
Proof. reflexivity. Qed.

Lemma unlift_preserves_calls :
  forall ps oid a al,
    unlift_stmt ps (Scall oid a al) =
    Scall oid (unlift_expr ps a) (List.map (unlift_expr ps) al).
Proof. reflexivity. Qed.

Lemma unlift_preserves_return_none :
  forall ps, unlift_stmt ps (Sreturn None) = Sreturn None.
Proof. reflexivity. Qed.

Lemma unlift_preserves_return_some :
  forall ps a,
    unlift_stmt ps (Sreturn (Some a)) = Sreturn (Some (unlift_expr ps a)).
Proof. reflexivity. Qed.

Lemma unlift_function_same_signature :
  forall f,
    fn_return (unlift_function f) = fn_return f /\
    fn_callconv (unlift_function f) = fn_callconv f /\
    fn_params (unlift_function f) = fn_params f /\
    fn_vars (unlift_function f) = fn_vars f /\
    fn_temps (unlift_function f) = fn_temps f.
Proof. intros f; repeat split; reflexivity. Qed.

Lemma store_params_cons :
  forall id ty rest s,
    store_params ((id, ty) :: rest) s =
    Ssequence (Sassign (Evar id ty) (Etempvar id ty))
              (store_params rest s).
Proof. reflexivity. Qed.

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
