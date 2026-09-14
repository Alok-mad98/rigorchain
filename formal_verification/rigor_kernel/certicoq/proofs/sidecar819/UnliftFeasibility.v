(** TASK 3.1C: feasibility facts for
    [forward_simulation (semantics2 original) (semantics1 core)].

    Not a completed simulation. No Admitted. No new Axiom.
    Official [function_entry1]/[function_entry2] only — no hybrid.
*)
From Coq Require Import List.
From compcert Require Import Coqlib Maps Integers Values Memory AST
  Globalenvs Events Ctypes Cop Clight.
From Phase10 Require Import UnliftDef UnliftRelation.

Local Open Scope list_scope.

(* ------------------------------------------------------------------ *)
(* Phase A — official contracts, by inversion of Clight.v 703–734.     *)
(* ------------------------------------------------------------------ *)

Lemma Semantics2EntryContract :
  forall ge f vargs m e le m',
    function_entry2 ge f vargs m e le m' ->
    list_norepet (var_names f.(fn_vars)) /\
    list_norepet (var_names f.(fn_params)) /\
    list_disjoint (var_names f.(fn_params)) (var_names f.(fn_temps)) /\
    alloc_variables ge empty_env m f.(fn_vars) e m' /\
    bind_parameter_temps f.(fn_params) vargs
      (create_undef_temps f.(fn_temps)) = Some le.
Proof.
  intros. inversion H. repeat split; assumption.
Qed.

Lemma Semantics1EntryContract :
  forall ge f vargs m e le m',
    function_entry1 ge f vargs m e le m' ->
    list_norepet (var_names f.(fn_params) ++ var_names f.(fn_vars)) /\
    (exists m1,
        alloc_variables ge empty_env m
          (f.(fn_params) ++ f.(fn_vars)) e m1 /\
        bind_parameters ge e m1 f.(fn_params) vargs m') /\
    le = create_undef_temps f.(fn_temps).
Proof.
  intros. inversion H. subst le.
  split; [assumption|].
  split; [|reflexivity].
  exists m1. split; assumption.
Qed.

Lemma bind_parameter_temps_other :
  forall ps vs le le' id,
    bind_parameter_temps ps vs le = Some le' ->
    ~ In id (var_names ps) ->
    le' ! id = le ! id.
Proof.
  induction ps as [| [id1 ty1] ps IH]; intros vs le le' id Hb Hnin.
  - destruct vs; simpl in Hb; inversion Hb. reflexivity.
  - destruct vs as [| v vs]; simpl in Hb; [discriminate|].
    apply IH with (id := id) in Hb.
    + rewrite Hb. rewrite PTree.gso; auto.
      intro Heq. subst. apply Hnin. left. reflexivity.
    + intro Hin. apply Hnin. right. exact Hin.
Qed.

Lemma bind_parameter_temps_head :
  forall id ty ps v vs le le',
    bind_parameter_temps ((id, ty) :: ps) (v :: vs) le = Some le' ->
    ~ In id (var_names ps) ->
    le' ! id = Some v.
Proof.
  intros id ty ps v vs le le' Hb Hnin.
  simpl in Hb.
  erewrite bind_parameter_temps_other; eauto.
  apply PTree.gss.
Qed.

(** C1: under [function_entry2], a parameter’s value is the
    corresponding argument, stored in the temp env. *)
Lemma function_entry2_param_temp_head :
  forall ge f m e le m' id ty ps v vs,
    function_entry2 ge f (v :: vs) m e le m' ->
    fn_params f = (id, ty) :: ps ->
    list_norepet (id :: var_names ps) ->
    le ! id = Some v.
Proof.
  intros ge f m e le m' id ty ps v vs Hent Heq Hnr.
  inversion Hent.
  rewrite Heq in H3.
  inversion Hnr. subst.
  eapply bind_parameter_temps_head; eauto.
Qed.

Lemma alloc_variables_in :
  forall ge e m vars e' m' id ty,
    alloc_variables ge e m vars e' m' ->
    list_norepet (var_names vars) ->
    In (id, ty) vars ->
    exists b, e' ! id = Some (b, ty).
Proof.
  intros ge e m vars e' m' id ty Halloc.
  induction Halloc as [| e m id1 ty1 vars m1 b1 m2 e2 Hm Hrest IH];
    intros Hnr Hin.
  - contradiction.
  - simpl in Hnr. inversion Hnr. subst.
    destruct Hin as [Heq | Hin].
    + inversion Heq. subst id1 ty1.
      exists b1.
      rewrite <- (alloc_variables_other ge (PTree.set id (b1, ty) e)
                   m1 vars e2 m2 Hrest id H1).
      apply PTree.gss.
    + apply IH; assumption.
Qed.

(** C2: under [function_entry1], a parameter has a freshly allocated
    local block in [e]. *)
Lemma var_names_app :
  forall a b, var_names (a ++ b) = var_names a ++ var_names b.
Proof.
  intros. unfold var_names. apply map_app.
Qed.

Lemma function_entry1_param_block :
  forall ge f vargs m e le m' id ty,
    function_entry1 ge f vargs m e le m' ->
    In (id, ty) (fn_params f) ->
    exists b, e ! id = Some (b, ty).
Proof.
  intros ge f vargs m e le m' id ty Hent Hin.
  inversion Hent.
  eapply alloc_variables_in; eauto.
  - rewrite var_names_app. exact H.
  - apply in_or_app. left. exact Hin.
Qed.

Lemma blocks_of_env_has :
  forall (ge: genv) e id b ty,
    e ! id = Some (b, ty) ->
    In (b, 0, sizeof ge ty) (blocks_of_env ge e).
Proof.
  intros ge e id b ty H.
  unfold blocks_of_env, block_of_binding.
  apply in_map_iff.
  exists (id, (b, ty)). split; [reflexivity|].
  apply PTree.elements_correct. exact H.
Qed.

(* ------------------------------------------------------------------ *)
(* Phase B — minimal state relation (regular states only).             *)
(* Call / return need extra memory-injection; not claimed here.        *)
(* ------------------------------------------------------------------ *)

Definition unlift_stmt_related (ps: list (ident * type))
    (s_src s_tgt: statement) : Prop :=
  s_tgt = unlift_stmt ps s_src.

Inductive unlift_state_related (ge: genv) (ps: list (ident * type))
    : state -> state -> Prop :=
| unlift_state_related_intro :
    forall f f' s k e1 le1 m1 e2 le2 m2,
      fn_params f = ps ->
      fn_params f' = ps ->
      fn_return f' = fn_return f ->
      fn_vars f' = fn_vars f ->
      fn_temps f' = fn_temps f ->
      unlift_env_related ge ps e1 le1 m1 e2 le2 m2 ->
      unlift_state_related ge ps
        (State f s k e1 le1 m1)
        (State f' (unlift_stmt ps s) k e2 le2 m2).

(* ------------------------------------------------------------------ *)
(* Phase E — one-step embeds into official step2 / step1.              *)
(* Same model preconditions as 3.1B. Not function simulation.          *)
(* ------------------------------------------------------------------ *)

Lemma exec_sset_step2_vs_step1 :
  forall ge f f' k e1 le1 m1 e2 le2 m2 ps id a ty chunk b v m2',
    param_type ps id = Some ty ->
    access_mode ty = By_value chunk ->
    e2 ! id = Some (b, ty) ->
    eval_expr ge e1 le1 m1 a v ->
    eval_expr ge e2 le2 m2 (unlift_expr ps a) v ->
    sem_cast v (typeof (unlift_expr ps a)) ty m2 = Some v ->
    Mem.storev chunk m2 (Vptr b Ptrofs.zero) v = Some m2' ->
    Val.load_result chunk v = v ->
    step2 ge (State f (Sset id a) k e1 le1 m1) E0
          (State f Sskip k e1 (PTree.set id v le1) m1) /\
    step1 ge
          (State f' (Sassign (Evar id ty) (unlift_expr ps a)) k e2 le2 m2) E0
          (State f' Sskip k e2 le2 m2') /\
    (PTree.set id v le1) ! id = Some v /\
    deref_loc ty m2' b Ptrofs.zero Full v.
Proof.
  intros ge f f' k e1 le1 m1 e2 le2 m2 ps id a ty chunk b v m2'
         Hty Hacc Hb He1 He2 Hcast Hst Hlr.
  unfold step2, step1.
  split.
  { apply step_set. exact He1. }
  split.
  { apply step_assign with (loc := b) (ofs := Ptrofs.zero) (bf := Full)
      (v2 := v) (v := v) (m' := m2').
    - apply eval_Evar_local. exact Hb.
    - exact He2.
    - exact Hcast.
    - apply assign_loc_value with (chunk := chunk).
      + exact Hacc.
      + exact Hst. }
  split.
  - apply PTree.gss.
  - eapply deref_loc_after_storev; eauto.
Qed.

Lemma exec_sequence_step2_vs_step1 :
  forall ge ps f f' k e1 le1 m1 e2 le2 m2 s1 s2,
    step2 ge (State f (Ssequence s1 s2) k e1 le1 m1) E0
          (State f s1 (Kseq s2 k) e1 le1 m1) /\
    step1 ge
          (State f' (unlift_stmt ps (Ssequence s1 s2)) k e2 le2 m2) E0
          (State f' (unlift_stmt ps s1) (Kseq (unlift_stmt ps s2) k) e2 le2 m2).
Proof.
  intros. unfold step2, step1. split.
  - apply step_seq.
  - rewrite unlift_preserves_control_structure. apply step_seq.
Qed.

(* ------------------------------------------------------------------ *)
(* Phase F — return / free_list shape, not inject preservation.        *)
(* ------------------------------------------------------------------ *)

Lemma step_return_1_shape :
  forall ge fe f a k e le m v v' m',
    eval_expr ge e le m a v ->
    sem_cast v (typeof a) f.(fn_return) m = Some v' ->
    Mem.free_list m (blocks_of_env ge e) = Some m' ->
    step ge fe (State f (Sreturn (Some a)) k e le m) E0
         (Returnstate v' (call_cont k) m').
Proof.
  intros. eapply step_return_1; eauto.
Qed.

Lemma entry2_param_not_in_free_list :
  forall ge f vargs m e le m' (id: ident),
    function_entry2 ge f vargs m e le m' ->
    ~ In id (var_names (fn_vars f)) ->
    e ! id = None.
Proof.
  intros. eapply function_entry2_param_not_local; eauto.
Qed.

Lemma entry1_param_is_in_free_list :
  forall ge f vargs m e le m' id ty,
    function_entry1 ge f vargs m e le m' ->
    In (id, ty) (fn_params f) ->
    exists b,
      e ! id = Some (b, ty) /\
      In (b, 0, sizeof ge ty) (blocks_of_env ge e).
Proof.
  intros ge f vargs m e le m' id ty Hent Hin.
  destruct (function_entry1_param_block ge f vargs m e le m' id ty Hent Hin)
    as [b Hb].
  exists b. split; [exact Hb|].
  eapply blocks_of_env_has; eauto.
Qed.

(** After [function_entry], source [blocks_of_env] is only [fn_vars];
    target includes every parameter block. [free_list] therefore
    frees extra blocks on the [semantics1] side. Relating the two
    memories after free requires [Mem.inject] +
    [SimplLocalsproof.match_envs_free_blocks]-scale reasoning.
    This file does not prove that. *)
Definition RETURN_FREE_LIST_REQUIRES_UPSTREAM_PROOF : bool := true.
