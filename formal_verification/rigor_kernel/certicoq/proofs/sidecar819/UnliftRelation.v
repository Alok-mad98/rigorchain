(** TASK 3.1B: Clight environment relation and official-entry blocker.

    CompCert 3.13 [Clight.v]:
    - [function_entry2] / [step2] / [semantics2]: parameters are temps
      ([bind_parameter_temps], [alloc_variables] on [fn_vars] only).
    - [function_entry1] / [step1] / [semantics1]: parameters are memory
      locals ([alloc_variables] on [fn_params ++ fn_vars],
      [bind_parameters] stores args; [le = create_undef_temps fn_temps]).
    - [eval_Etempvar]: [le!id = Some v] ([Clight.v:377-379]).
    - [eval_Evar_local]: [e!id = Some (l, ty)] ([Clight.v:411-413]).
    - [step_set]: updates temps, memory unchanged ([Clight.v:569-572]).
    - [step_assign]: [assign_loc] updates memory, temps unchanged
      ([Clight.v:561-567]).

    CertiCoq-generated Clight uses [Etempvar]/[Sset] on [fn_params]
    and therefore matches [semantics2].

    [unlift_stmt] rewrites those to [Evar]/[Sassign], which matches
    [semantics1] (the SimplLocals source convention).

    [store_params] in [unlift_function] is
    [Sassign (Evar id ty) (Etempvar id ty)]:
    - under [function_entry1], [Etempvar id] is not in [le];
    - under [function_entry2], [Evar id] is not in [e].
    CompCert does not define a hybrid entry. That is the
    function-level simulation boundary.
*)
From Coq Require Import List.
From compcert Require Import Coqlib Maps Integers Values Memory AST
  Globalenvs Events Ctypes Cop Clight.
From Phase10 Require Import UnliftDef.

Local Open Scope list_scope.

Lemma param_type_cons :
  forall id' ty' ps id,
    param_type ((id', ty') :: ps) id =
    match ident_eq id' id with
    | left _ => Some ty'
    | right _ => param_type ps id
    end.
Proof.
  intros. unfold param_type. simpl.
  destruct (ident_eq id' id); reflexivity.
Qed.

Lemma param_type_Some_In :
  forall ps id ty,
    param_type ps id = Some ty ->
    In (id, ty) ps.
Proof.
  induction ps as [| [id' ty'] ps IH]; intros id ty H.
  - unfold param_type in H. simpl in H. discriminate.
  - rewrite param_type_cons in H.
    destruct (ident_eq id' id).
    + inversion H. subst. left. reflexivity.
    + right. apply IH. exact H.
Qed.

Lemma in_params_in_names :
  forall (ps: list (ident * type)) id ty,
    In (id, ty) ps -> In id (var_names ps).
Proof.
  induction ps as [| [id' ty'] ps IH]; simpl; intros id ty H.
  - contradiction.
  - destruct H as [H | H].
    + inversion H. subst. left. reflexivity.
    + right. eapply IH; eauto.
Qed.

Lemma create_undef_temps_notin :
  forall temps id,
    ~ In id (var_names temps) ->
    (create_undef_temps temps) ! id = None.
Proof.
  induction temps as [| [id1 ty1] temps IH]; simpl; intros id Hnin.
  - apply PTree.gempty.
  - rewrite PTree.gso.
    + apply IH. intro Hin. apply Hnin. right. exact Hin.
    + intro Heq. subst. apply Hnin. left. reflexivity.
Qed.

Lemma alloc_variables_other :
  forall ge e m vars e' m',
    alloc_variables ge e m vars e' m' ->
    forall id0, ~ In id0 (var_names vars) -> e ! id0 = e' ! id0.
Proof.
  intros ge e m vars e' m' Halloc.
  induction Halloc as [| e m id ty vars m1 b1 m2 e2 Hallocm Hrest IH];
    simpl; intros id0 Hnin; auto.
  replace (e ! id0) with ((PTree.set id (b1, ty) e) ! id0).
  apply IH. intro Hin. apply Hnin. right. exact Hin.
  rewrite PTree.gsspec. destruct (peq id0 id) as [Heq | Hne].
  subst id0. exfalso. apply Hnin. left. reflexivity.
  reflexivity.
Qed.

(** Official [function_entry1]: parameters are not in the temp env
    unless they also appear in [fn_temps]. *)
Lemma function_entry1_param_temp_none :
  forall ge f vargs m e le m' id ty,
    function_entry1 ge f vargs m e le m' ->
    In (id, ty) (fn_params f) ->
    ~ In id (var_names (fn_temps f)) ->
    le ! id = None.
Proof.
  intros ge f vargs m e le m' id ty Hent Hin Hnin.
  inversion Hent. subst le.
  apply create_undef_temps_notin. exact Hnin.
Qed.

Lemma eval_Etempvar_inv :
  forall ge e le m id ty v,
    eval_expr ge e le m (Etempvar id ty) v ->
    le ! id = Some v.
Proof.
  intros ge e le m id ty v H.
  inversion H; subst; auto.
  inversion H0.
Qed.

(** [store_params] RHS cannot evaluate under [function_entry1]. *)
Lemma store_params_temp_stuck_entry1 :
  forall ge f vargs m e le m' id ty,
    function_entry1 ge f vargs m e le m' ->
    In (id, ty) (fn_params f) ->
    ~ In id (var_names (fn_temps f)) ->
    forall v, ~ eval_expr ge e le m' (Etempvar id ty) v.
Proof.
  intros ge f vargs m e le m' id ty Hent Hin Hnin v Heval.
  apply eval_Etempvar_inv in Heval.
  erewrite function_entry1_param_temp_none in Heval; eauto.
  discriminate.
Qed.

(** Official [function_entry2]: parameters are not in the local env
    unless they also appear in [fn_vars]. *)
Lemma function_entry2_param_not_local :
  forall ge f vargs m e le m' (id: ident),
    function_entry2 ge f vargs m e le m' ->
    ~ In id (var_names (fn_vars f)) ->
    e ! id = None.
Proof.
  intros ge f vargs m e le m' id Hent Hnin.
  inversion Hent.
  rewrite <- (alloc_variables_other ge empty_env m (fn_vars f) e m' H2 id Hnin).
  unfold empty_env. apply PTree.gempty.
Qed.

Lemma store_params_evar_not_local_entry2 :
  forall ge f vargs m e le m' id ty loc ofs bf,
    function_entry2 ge f vargs m e le m' ->
    ~ In id (var_names (fn_vars f)) ->
    eval_lvalue ge e le m' (Evar id ty) loc ofs bf ->
    e ! id = None /\ Genv.find_symbol ge id = Some loc.
Proof.
  intros ge f vargs m e le m' id ty loc ofs bf Hent Hnin Hl.
  pose proof (function_entry2_param_not_local ge f vargs m e le m' id Hent Hnin) as Hnone.
  inversion Hl; subst.
  - congruence.
  - split; auto.
Qed.

(** Simulation relation: original (Clight2 / temps) vs unlifted
    (Clight1 / memory locals) for the parameter set [ps].

    Model preconditions (not hidden):
    - this relation does *not* assert [m1]/[m2] agree on shared
      blocks; general [eval_expr] of non-param expressions is
      therefore an extra hypothesis at each use;
    - parameter types are expected to be [By_value] scalars
      (CertiCoq checker params are [tlong] / pointers). *)
Definition unlift_env_related
    (ge: genv) (ps: list (ident * type))
    (e1: env) (le1: temp_env) (m1: mem)
    (e2: env) (le2: temp_env) (m2: mem) : Prop :=
  (forall id ty,
      param_type ps id = Some ty ->
      exists b,
        e2 ! id = Some (b, ty) /\
        (forall v, le1 ! id = Some v <->
                   deref_loc ty m2 b Ptrofs.zero Full v)) /\
  (forall id, param_type ps id = None -> e1 ! id = e2 ! id) /\
  (forall id, param_type ps id = None -> le1 ! id = le2 ! id).

Lemma eval_param_as_temp :
  forall ge ps e1 le1 m1 e2 le2 m2 id ty v,
    unlift_env_related ge ps e1 le1 m1 e2 le2 m2 ->
    param_type ps id = Some ty ->
    le1 ! id = Some v ->
    eval_expr ge e1 le1 m1 (Etempvar id ty) v.
Proof.
  intros. apply eval_Etempvar. exact H1.
Qed.

Lemma eval_param_as_var :
  forall ge ps e1 le1 m1 e2 le2 m2 id ty v,
    unlift_env_related ge ps e1 le1 m1 e2 le2 m2 ->
    param_type ps id = Some ty ->
    le1 ! id = Some v ->
    eval_expr ge e2 le2 m2 (Evar id ty) v.
Proof.
  intros ge ps e1 le1 m1 e2 le2 m2 id ty v [Hpar _] Hty Hv.
  destruct (Hpar id ty Hty) as [b [Hb Hiff]].
  eapply eval_Elvalue.
  - eapply eval_Evar_local. exact Hb.
  - simpl. apply Hiff. exact Hv.
Qed.

(** Core observable: [Etempvar id] on the original side and
    [Evar id] on the unlifted side denote the same value when
    [id] is a parameter and the relation holds. *)
Lemma eval_unlifted_param_equiv :
  forall ge ps e1 le1 m1 e2 le2 m2 id ty v,
    unlift_env_related ge ps e1 le1 m1 e2 le2 m2 ->
    param_type ps id = Some ty ->
    le1 ! id = Some v ->
    eval_expr ge e1 le1 m1 (Etempvar id ty) v /\
    eval_expr ge e2 le2 m2 (Evar id ty) v.
Proof.
  intros. split.
  - eapply eval_param_as_temp; eauto.
  - eapply eval_param_as_var; eauto.
Qed.

Lemma deref_loc_after_storev :
  forall ty chunk m b v m',
    access_mode ty = By_value chunk ->
    Mem.storev chunk m (Vptr b Ptrofs.zero) v = Some m' ->
    Val.load_result chunk v = v ->
    deref_loc ty m' b Ptrofs.zero Full v.
Proof.
  intros ty chunk m b v m' Hacc Hst Hlr.
  eapply deref_loc_value; eauto.
  unfold Mem.storev, Mem.loadv in *.
  erewrite Mem.load_store_same; eauto.
  rewrite Hlr. reflexivity.
Qed.

(** One-step: original [Sset] of a parameter vs unlifted
    [Sassign (Evar param)].

    Model preconditions written in the statement:
    - [unlift_env_related];
    - both right-hand sides evaluate to the same [v]
      (this file does not prove general [eval_expr] commutation);
    - [sem_cast] is identity at this type/memory;
    - the target param block admits [Mem.storev];
    - [Val.load_result] is identity (true for [Vlong]/[Mint64]). *)
Lemma exec_sset_to_sassign_steps :
  forall ge (fe: function -> list val -> mem -> env -> temp_env -> mem -> Prop)
         f f' k e1 le1 m1 e2 le2 m2 ps id a ty chunk b v m2',
    param_type ps id = Some ty ->
    access_mode ty = By_value chunk ->
    e2 ! id = Some (b, ty) ->
    eval_expr ge e1 le1 m1 a v ->
    eval_expr ge e2 le2 m2 (unlift_expr ps a) v ->
    sem_cast v (typeof (unlift_expr ps a)) ty m2 = Some v ->
    Mem.storev chunk m2 (Vptr b Ptrofs.zero) v = Some m2' ->
    step ge fe (State f (Sset id a) k e1 le1 m1) E0
         (State f Sskip k e1 (PTree.set id v le1) m1) /\
    step ge fe
         (State f' (Sassign (Evar id ty) (unlift_expr ps a)) k e2 le2 m2) E0
         (State f' Sskip k e2 le2 m2').
Proof.
  intros ge fe f f' k e1 le1 m1 e2 le2 m2 ps id a ty chunk b v m2'
         Hty Hacc Hb He1 He2 Hcast Hst.
  split.
  - apply step_set. exact He1.
  - apply step_assign with (loc := b) (ofs := Ptrofs.zero) (bf := Full)
      (v2 := v) (v := v) (m' := m2').
    + apply eval_Evar_local. exact Hb.
    + exact He2.
    + exact Hcast.
    + apply assign_loc_value with (chunk := chunk).
      * exact Hacc.
      * exact Hst.
Qed.

Lemma exec_sset_to_sassign_observable :
  forall ty chunk m2 b v m2',
    access_mode ty = By_value chunk ->
    Mem.storev chunk m2 (Vptr b Ptrofs.zero) v = Some m2' ->
    Val.load_result chunk v = v ->
    (forall id (le1: temp_env), (PTree.set id v le1) ! id = Some v) /\
    deref_loc ty m2' b Ptrofs.zero Full v.
Proof.
  intros. split.
  - intros. apply PTree.gss.
  - eapply deref_loc_after_storev; eauto.
Qed.

Lemma exec_sset_to_sassign_simulation :
  forall ge (fe: function -> list val -> mem -> env -> temp_env -> mem -> Prop)
         f f' k e1 le1 m1 e2 le2 m2 ps id a ty chunk b v m2',
    param_type ps id = Some ty ->
    access_mode ty = By_value chunk ->
    e2 ! id = Some (b, ty) ->
    eval_expr ge e1 le1 m1 a v ->
    eval_expr ge e2 le2 m2 (unlift_expr ps a) v ->
    sem_cast v (typeof (unlift_expr ps a)) ty m2 = Some v ->
    Mem.storev chunk m2 (Vptr b Ptrofs.zero) v = Some m2' ->
    Val.load_result chunk v = v ->
    step ge fe (State f (Sset id a) k e1 le1 m1) E0
         (State f Sskip k e1 (PTree.set id v le1) m1) /\
    step ge fe
         (State f' (Sassign (Evar id ty) (unlift_expr ps a)) k e2 le2 m2) E0
         (State f' Sskip k e2 le2 m2') /\
    (PTree.set id v le1) ! id = Some v /\
    deref_loc ty m2' b Ptrofs.zero Full v.
Proof.
  intros.
  destruct (exec_sset_to_sassign_steps ge fe f f' k e1 le1 m1 e2 le2 m2
              ps id a ty chunk b v m2' H H0 H1 H2 H3 H4 H5) as [S1 S2].
  destruct (exec_sset_to_sassign_observable ty chunk m2 b v m2' H0 H5 H6)
    as [Ht Hd].
  repeat split; auto.
Qed.

Lemma exec_sequence_unlift_simulation :
  forall ge (fe: function -> list val -> mem -> env -> temp_env -> mem -> Prop)
         ps f k e le m s1 s2,
    step ge fe (State f (Ssequence s1 s2) k e le m) E0
         (State f s1 (Kseq s2 k) e le m) /\
    step ge fe
         (State f (unlift_stmt ps (Ssequence s1 s2)) k e le m) E0
         (State f (unlift_stmt ps s1) (Kseq (unlift_stmt ps s2) k) e le m).
Proof.
  intros. split.
  - apply step_seq.
  - rewrite unlift_preserves_control_structure. apply step_seq.
Qed.

Lemma exec_skip_unlift_id :
  forall ps, unlift_stmt ps Sskip = Sskip.
Proof. reflexivity. Qed.

Lemma exec_break_unlift_id :
  forall ps, unlift_stmt ps Sbreak = Sbreak.
Proof. reflexivity. Qed.

Lemma exec_continue_unlift_id :
  forall ps, unlift_stmt ps Scontinue = Scontinue.
Proof. reflexivity. Qed.

Lemma tlong_access_mode :
  access_mode (Tlong Signed noattr) = By_value Mint64.
Proof. reflexivity. Qed.
