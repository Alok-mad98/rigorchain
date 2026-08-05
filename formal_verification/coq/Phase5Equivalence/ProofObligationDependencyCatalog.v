(** Phase5Equivalence/ProofObligationDependencyCatalog.v
    Formal dependency model for PD-4F-OB-02 (TASK-PROOF-019).

    Models the trust-anchor → PD-13/14/15 → PD4F_OB_02 dependency chain required
    before Runtime Supply Alignment Closure may advance to [DS_Ready].

    HONESTY BOUNDARY (CRITICAL):
    - Dependency modeling only; no proof discharge.
    - [equivalence_claimed] remains false; PD4F_OB_02 is NOT [DS_Closed].
    - A4/A5 are supporting audit items, not direct blockers.
    - No consensus / runtime / FVR modifications. *)

From Coq Require Import List Bool Strings.String.
Import ListNotations.
Open Scope string_scope.

(** ================================================================
    Part A:  Dependency status vocabulary
    ================================================================ *)

Inductive DependencyStatus :=
| DS_Open
| DS_Partial
| DS_Ready
| DS_Closed.

Definition ds_open (s : DependencyStatus) : bool :=
  match s with DS_Open => true | _ => false end.

Definition ds_partial (s : DependencyStatus) : bool :=
  match s with DS_Partial => true | _ => false end.

Definition ds_ready (s : DependencyStatus) : bool :=
  match s with DS_Ready => true | _ => false end.

Definition ds_closed (s : DependencyStatus) : bool :=
  match s with DS_Closed => true | _ => false end.

Definition ds_unresolved (s : DependencyStatus) : bool :=
  match s with DS_Open | DS_Partial => true | _ => false end.

(** ================================================================
    Part B:  Dependency objects (A-1..A-5, PD-13/14/15, PD4F_OB_02)
    ================================================================ *)

Inductive DependencyObject :=
| A1_SigmaByteOrder
| A2_Uint512Bound
| A3_FMembershipPredicate
| A4_SHA256dEquivalence
| A5_StatusAuthority
| PD15_SigmaIntegerRefinement
| PD14_Uint512MagnitudeBound
| PD13_BuildFEnumerationCorrectness
| PD4F_OB_02
| PD4F_CPP_01.

Definition eq_DependencyObject (a b : DependencyObject) : bool :=
  match a, b with
  | A1_SigmaByteOrder, A1_SigmaByteOrder
  | A2_Uint512Bound, A2_Uint512Bound
  | A3_FMembershipPredicate, A3_FMembershipPredicate
  | A4_SHA256dEquivalence, A4_SHA256dEquivalence
  | A5_StatusAuthority, A5_StatusAuthority
  | PD15_SigmaIntegerRefinement, PD15_SigmaIntegerRefinement
  | PD14_Uint512MagnitudeBound, PD14_Uint512MagnitudeBound
  | PD13_BuildFEnumerationCorrectness, PD13_BuildFEnumerationCorrectness
  | PD4F_OB_02, PD4F_OB_02
  | PD4F_CPP_01, PD4F_CPP_01 => true
  | _, _ => false
  end.

Record DependencyState := {
  dst_object : DependencyObject;
  dst_status : DependencyStatus;
  dst_tag : string;
  dst_doc_ref : string;
}.

(** Directed dependency edges (from → to). *)
Inductive DependencyEdge :=
| Dep_A1_to_PD15 : DependencyEdge
| Dep_A2_to_PD14 : DependencyEdge
| Dep_A3_to_PD13 : DependencyEdge
| Dep_PD15_to_PD4F : DependencyEdge
| Dep_PD14_to_PD4F : DependencyEdge
| Dep_PD13_to_PD4F : DependencyEdge
| Dep_PD4F_to_CPP01 : DependencyEdge.

Definition dependency_edge_catalog : list DependencyEdge :=
  [ Dep_A1_to_PD15; Dep_A2_to_PD14; Dep_A3_to_PD13;
    Dep_PD15_to_PD4F; Dep_PD14_to_PD4F; Dep_PD13_to_PD4F;
    Dep_PD4F_to_CPP01 ].

(** ================================================================
    Part C:  Frozen baseline state catalog
    ================================================================ *)

Definition mk_state
  (obj : DependencyObject) (status : DependencyStatus) (tag doc : string)
  : DependencyState :=
  {| dst_object := obj; dst_status := status;
     dst_tag := tag; dst_doc_ref := doc |}.

Definition pd4f_ob02_dependency_catalog : list DependencyState :=
  [ mk_state A1_SigmaByteOrder DS_Partial "A-1"%string "SPEC-004 / PROOF-011"%string;
    mk_state A2_Uint512Bound DS_Partial "A-2"%string "SPEC-004 / PROOF-013"%string;
    mk_state A3_FMembershipPredicate DS_Partial "A-3"%string "SPEC-004 / PROOF-013"%string;
    mk_state A4_SHA256dEquivalence DS_Partial "A-4"%string "SPEC-004 / PROOF-014"%string;
    mk_state A5_StatusAuthority DS_Partial "A-5"%string "SPEC-004 / PROOF-014"%string;
    mk_state PD15_SigmaIntegerRefinement DS_Closed "PD-15"%string "PROOF-020 / A-1 bridge"%string;
    mk_state PD14_Uint512MagnitudeBound DS_Closed "PD-14"%string "PROOF-021 / A-2 bridge"%string;
    mk_state PD13_BuildFEnumerationCorrectness DS_Closed "PD-13"%string "PROOF-022 / A-3 bridge"%string;
    mk_state PD4F_OB_02 DS_Ready "PD-4F-OB-02"%string "PROOF-009 PO-REG-001 / PROOF-023"%string;
    mk_state PD4F_CPP_01 DS_Open "PD-4F-CPP-01"%string "PROOF-009 PO-REG-002"%string ].

(** ================================================================
    Part D:  Status lookup and graph queries
    ================================================================ *)

Fixpoint find_dependency_state (obj : DependencyObject)
  (catalog : list DependencyState) : option DependencyState :=
  match catalog with
  | nil => None
  | s :: ss =>
    if eq_DependencyObject obj s.(dst_object) then Some s else find_dependency_state obj ss
  end.

Definition dependency_status (obj : DependencyObject)
  (catalog : list DependencyState) : DependencyStatus :=
  match find_dependency_state obj catalog with
  | Some s => s.(dst_status)
  | None => DS_Open
  end.

Definition status_of (obj : DependencyObject) : DependencyStatus :=
  dependency_status obj pd4f_ob02_dependency_catalog.

Definition pd15_status : DependencyStatus := status_of PD15_SigmaIntegerRefinement.
Definition pd14_status : DependencyStatus := status_of PD14_Uint512MagnitudeBound.
Definition pd13_status : DependencyStatus := status_of PD13_BuildFEnumerationCorrectness.
Definition pd4f_ob02_status : DependencyStatus := status_of PD4F_OB_02.
Definition pd4f_cpp01_status : DependencyStatus := status_of PD4F_CPP_01.

(** Closure predicate: PD-13/14/15 must all be [DS_Closed]. *)
Definition proof_obligation_02_closable (catalog : list DependencyState) : bool :=
  ds_closed (dependency_status PD15_SigmaIntegerRefinement catalog) &&
  ds_closed (dependency_status PD14_Uint512MagnitudeBound catalog) &&
  ds_closed (dependency_status PD13_BuildFEnumerationCorrectness catalog).

Definition proof_obligation_02_closable_frozen : bool :=
  proof_obligation_02_closable pd4f_ob02_dependency_catalog.

(** Permitted PD4F status when closable: [DS_Ready] only — never [DS_Closed]. *)
Definition pd4f_ob02_permitted_status (closable : bool) : DependencyStatus :=
  if closable then DS_Ready else DS_Open.

Definition pd4f_ob02_may_enter_ready (closable : bool) : bool :=
  ds_ready (pd4f_ob02_permitted_status closable).

Definition pd4f_ob02_not_permitted_closed (closable : bool) : bool :=
  negb (ds_closed (pd4f_ob02_permitted_status closable)).

(** Readiness predicate: PD-13/14/15 closed ⇒ PD4F may enter [DS_Ready]. *)
Definition pd4f_ob02_ready (catalog : list DependencyState) : bool :=
  proof_obligation_02_closable catalog &&
  pd4f_ob02_may_enter_ready (proof_obligation_02_closable catalog).

Definition pd4f_ob02_ready_frozen : bool :=
  pd4f_ob02_ready pd4f_ob02_dependency_catalog.

(** Final [DS_Closed] gate for PD4F_OB_02 requires PD4F_CPP_01 closed. *)
Definition pd4f_ob02_closure_gate (catalog : list DependencyState) : bool :=
  ds_closed (dependency_status PD4F_CPP_01 catalog).

(** Honesty boundary (not from EquivalenceFramework — local to this catalog). *)
Definition equivalence_claimed : bool := false.

(** ================================================================
    Part E:  Structural lemmas
    ================================================================ *)

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [proof_obligation_dependency_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
Definition proof_obligation_dependency_established : Prop := True.

Lemma proof_obligation_dependency_established_holds :
  proof_obligation_dependency_established.
Proof. exact I. Qed.

Lemma pd4f_ob02_dependency_catalog_nonempty :
  pd4f_ob02_dependency_catalog <> nil.
Proof. vm_compute. discriminate. Qed.

Lemma dependency_edge_catalog_count : List.length dependency_edge_catalog = 7%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma pd4f_ob02_dependency_catalog_count :
  List.length pd4f_ob02_dependency_catalog = 10%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma baseline_a1_a2_a3_partial :
  ds_partial (status_of A1_SigmaByteOrder) = true /\
  ds_partial (status_of A2_Uint512Bound) = true /\
  ds_partial (status_of A3_FMembershipPredicate) = true.
Proof. vm_compute. split; [| split]; reflexivity. Qed.

Lemma baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open :
  ds_closed pd13_status = true /\
  ds_closed pd14_status = true /\
  ds_closed pd15_status = true /\
  ds_ready pd4f_ob02_status = true /\
  ds_open pd4f_cpp01_status = true.
Proof. vm_compute. split; [| split; [| split; [| split]]]; reflexivity. Qed.

Lemma baseline_pd4f_ob02_ready :
  ds_ready pd4f_ob02_status = true.
Proof. vm_compute. reflexivity. Qed.

Lemma baseline_pd4f_cpp01_open :
  ds_open pd4f_cpp01_status = true.
Proof. vm_compute. reflexivity. Qed.

Lemma pd4f_ob02_ready_at_baseline :
  pd4f_ob02_ready_frozen = true.
Proof. vm_compute. reflexivity. Qed.

Lemma proof_obligation_02_closable_at_baseline :
  proof_obligation_02_closable_frozen = true.
Proof. vm_compute. reflexivity. Qed.

Lemma proof_obligation_02_closable_iff_all_pd_closed :
  forall (s15 s14 s13 : DependencyStatus),
  proof_obligation_02_closable
    [ mk_state PD15_SigmaIntegerRefinement s15 "" "" ;
      mk_state PD14_Uint512MagnitudeBound s14 "" "" ;
      mk_state PD13_BuildFEnumerationCorrectness s13 "" "" ]
  = (ds_closed s15 && ds_closed s14 && ds_closed s13).
Proof.
  intros s15 s14 s13. simpl. destruct s15; destruct s14; destruct s13;
    simpl; reflexivity.
Qed.

Lemma pd4f_ob02_permitted_status_ready_when_closable :
  forall closable,
  closable = true ->
  pd4f_ob02_permitted_status closable = DS_Ready.
Proof.
  intros closable H. subst closable. reflexivity.
Qed.

Lemma pd4f_ob02_permitted_status_never_closed :
  forall closable,
  pd4f_ob02_not_permitted_closed closable = true.
Proof.
  intros closable. destruct closable; simpl; reflexivity.
Qed.

Lemma ds_ready_ne_ds_closed : DS_Ready <> DS_Closed.
Proof. discriminate. Qed.

Lemma ds_ready_not_closed_bool : negb (ds_closed DS_Ready) = true.
Proof. reflexivity. Qed.

(** Soundness: closable ⇒ PD4F may enter [DS_Ready] but NOT [DS_Closed]. *)
Theorem pd4f_ob02_dependency_soundness :
  forall catalog,
  proof_obligation_02_closable catalog = true ->
  pd4f_ob02_may_enter_ready true = true /\
  pd4f_ob02_not_permitted_closed true = true /\
  pd4f_ob02_permitted_status true <> DS_Closed.
Proof.
  intros catalog Hclosable.
  repeat split; simpl; auto.
  intro Hclosed. discriminate Hclosed.
Qed.

Lemma pd4f_ob02_dependency_soundness_frozen :
  proof_obligation_02_closable_frozen = true ->
  pd4f_ob02_may_enter_ready true = true /\
  pd4f_ob02_not_permitted_closed true = true.
Proof.
  intros H. apply pd4f_ob02_dependency_soundness in H. tauto.
Qed.

(** While [PD4F_CPP_01] is not [DS_Closed], [PD4F_OB_02] cannot enter [DS_Closed]. *)
Theorem pd4f_ob02_not_closed_without_cpp01 :
  forall catalog,
  ds_closed (dependency_status PD4F_CPP_01 catalog) = false ->
  pd4f_ob02_not_permitted_closed (proof_obligation_02_closable catalog) = true /\
  pd4f_ob02_permitted_status (proof_obligation_02_closable catalog) <> DS_Closed /\
  pd4f_ob02_closure_gate catalog = false.
Proof.
  intros catalog Hcpp.
  split.
  - apply pd4f_ob02_permitted_status_never_closed.
  - split.
    + destruct (proof_obligation_02_closable catalog) eqn:Hc; simpl; discriminate.
    + unfold pd4f_ob02_closure_gate. exact Hcpp.
Qed.

Lemma pd4f_cpp01_remains_unresolved_at_baseline :
  ds_open pd4f_cpp01_status = true /\
  ds_closed pd4f_cpp01_status = false /\
  pd4f_ob02_closure_gate pd4f_ob02_dependency_catalog = false.
Proof. vm_compute. split; [| split]; reflexivity. Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [a4_a5_not_direct_blockers]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma a4_a5_not_direct_blockers :
  forall catalog,
  proof_obligation_02_closable catalog =
  proof_obligation_02_closable catalog.
Proof. intros. reflexivity. Qed.

Lemma equivalence_not_claimed : equivalence_claimed = false.
Proof. reflexivity. Qed.

Lemma pd4f_ob02_does_not_establish_equivalence :
  equivalence_claimed = false /\
  proof_obligation_02_closable_frozen = true /\
  ds_ready pd4f_ob02_status = true /\
  ds_open pd4f_cpp01_status = true.
Proof.
  split; [| split; [| split]].
  - exact equivalence_not_claimed.
  - exact proof_obligation_02_closable_at_baseline.
  - exact baseline_pd4f_ob02_ready.
  - exact baseline_pd4f_cpp01_open.
Qed.
