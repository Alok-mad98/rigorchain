(** TASK 3.1B: instantiate unlift facts on [f_f_case_118].

    Does not claim function-level or [check_oru]-level simulation.
*)
From Coq Require Import List.
From CheckOruGen Require Import check_oru_clight.
From Phase10 Require Import UnliftDef UnliftRelation.
From compcert Require Import Coqlib Integers Maps AST Globalenvs Ctypes Cop Clight Clightdefs.
Import Clightdefs.ClightNotations.
Local Open Scope clight_scope.

Definition env_restore_rhs : expr :=
  Ederef
    (Ebinop Oadd
      (Evar _root (tarray tlong 3))
      (Econst_long (Int64.repr 0) tlong)
      (tptr tlong))
    tlong.

Definition env_restore_sset : statement :=
  Sset __env_301 env_restore_rhs.

Lemma f_case_118_env_301_is_param :
  param_type (fn_params f_f_case_118) __env_301 = Some tlong.
Proof. vm_compute. reflexivity. Qed.

Definition names_mem (id: ident) (xs: list ident) : bool :=
  existsb (fun x => match ident_eq x id with left _ => true | right _ => false end) xs.

Lemma names_mem_false_not_in :
  forall id xs, names_mem id xs = false -> ~ In id xs.
Proof.
  intros id xs H Hin.
  assert (names_mem id xs = true) as Htrue.
  { unfold names_mem. apply existsb_exists.
    exists id. split; [exact Hin|].
    destruct (ident_eq id id) as [_|E]; [reflexivity|contradiction]. }
  congruence.
Qed.

Lemma f_case_118_env_301_not_temp :
  ~ In __env_301 (var_names (fn_temps f_f_case_118)).
Proof.
  apply names_mem_false_not_in.
  vm_compute. reflexivity.
Qed.

Lemma f_case_118_env_301_not_var :
  ~ In __env_301 (var_names (fn_vars f_f_case_118)).
Proof.
  apply names_mem_false_not_in.
  vm_compute. reflexivity.
Qed.

Lemma f_case_118_env_301_in_params :
  In (__env_301, tlong) (fn_params f_f_case_118).
Proof.
  eapply param_type_Some_In.
  apply f_case_118_env_301_is_param.
Qed.

Lemma unlift_env_restore_is_sassign :
  unlift_stmt (fn_params f_f_case_118) env_restore_sset =
  Sassign (Evar __env_301 tlong) (unlift_expr (fn_params f_f_case_118) env_restore_rhs).
Proof.
  unfold env_restore_sset.
  pose proof (unlift_sset_only_params (fn_params f_f_case_118) __env_301 env_restore_rhs) as H.
  rewrite f_case_118_env_301_is_param in H.
  exact H.
Qed.

Lemma env_restore_rhs_unlift_id :
  unlift_expr (fn_params f_f_case_118) env_restore_rhs = env_restore_rhs.
Proof. vm_compute. reflexivity. Qed.

Lemma tlong_is_by_value_mint64 :
  access_mode tlong = By_value Mint64.
Proof. reflexivity. Qed.

Lemma store_params_entry1_stuck_on_f_case_118 :
  forall ge vargs m e le m' v,
    function_entry1 ge f_f_case_118 vargs m e le m' ->
    ~ eval_expr ge e le m' (Etempvar __env_301 tlong) v.
Proof.
  intros ge vargs m e le m' v Hent.
  eapply store_params_temp_stuck_entry1; eauto.
  - apply f_case_118_env_301_in_params.
  - apply f_case_118_env_301_not_temp.
Qed.

Lemma store_params_entry2_evar_not_local_on_f_case_118 :
  forall ge vargs m e le m' loc ofs bf,
    function_entry2 ge f_f_case_118 vargs m e le m' ->
    eval_lvalue ge e le m' (Evar __env_301 tlong) loc ofs bf ->
    PTree.get __env_301 e = None /\ Genv.find_symbol ge __env_301 = Some loc.
Proof.
  intros.
  eapply store_params_evar_not_local_entry2; eauto.
  apply f_case_118_env_301_not_var.
Qed.

(** The 3.1A [unlift_function] body starts with
    [Sassign (Evar __tinfo _) (Etempvar __tinfo _)], then the same
    for [__env_301]. Official entries cannot justify that prefix. *)
Lemma unlift_function_f_case_118_starts_with_store_params :
  fn_body (unlift_function f_f_case_118) =
  store_params (fn_params f_f_case_118) (unlift_body f_f_case_118).
Proof. reflexivity. Qed.

Lemma unlift_function_core_f_case_118_no_store :
  fn_body (unlift_function_core f_f_case_118) =
  unlift_stmt (fn_params f_f_case_118) (fn_body f_f_case_118).
Proof. reflexivity. Qed.

(** Phase D instantiation: [__env_301] as temp vs local under the
    relation. Model precondition: [unlift_env_related] and the
    original temp holds [v]. *)
Lemma eval_env_301_temp_vs_var :
  forall ge e1 le1 m1 e2 le2 m2 v,
    unlift_env_related ge (fn_params f_f_case_118) e1 le1 m1 e2 le2 m2 ->
    PTree.get __env_301 le1 = Some v ->
    eval_expr ge e1 le1 m1 (Etempvar __env_301 tlong) v /\
    eval_expr ge e2 le2 m2 (Evar __env_301 tlong) v.
Proof.
  intros ge e1 le1 m1 e2 le2 m2 v Hrel Hv.
  apply (eval_unlifted_param_equiv ge (fn_params f_f_case_118)
           e1 le1 m1 e2 le2 m2 __env_301 tlong v Hrel
           f_case_118_env_301_is_param Hv).
Qed.
