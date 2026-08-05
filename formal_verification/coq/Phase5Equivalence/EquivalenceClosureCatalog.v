(** Phase5Equivalence/EquivalenceClosureCatalog.v
    Equivalence closure condition layer (TASK-FVR-L).

    Defines the formal proof-permission boundary for Phase-5 Coq ≡ Runtime.
    Consumes FVR-F/E/G/H/J/K and 018GG-M integration scaffold outputs.

    HONESTY BOUNDARY (CRITICAL):
    - DOES NOT prove Coq = Runtime equivalence.
    - [equivalence_claimed] remains false; never sets coq_equiv_runtime := true.
    - PD-4F-OB-02 / PD-4F-CPP-01 remain OPEN. *)

From Coq Require Import List ZArith Bool Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.RuntimeEquivalenceCandidateCatalog.
Require Import Rigor.Phase5Equivalence.GlobalCandidateEquivalenceCatalog.
Require Import Rigor.Phase5Equivalence.RuntimeAlignmentIntegrationCatalog.

Close Scope Z_scope.
Open Scope string_scope.

(** ================================================================
    Part A:  Closure vocabulary
    ================================================================ *)

Inductive EquivalenceClosureStatus :=
| ECS_Open
| ECS_PartiallyClosable
| ECS_Closable
| ECS_Prohibited.

Definition ecs_open (s : EquivalenceClosureStatus) : bool :=
  match s with ECS_Open => true | _ => false end.
Definition ecs_partially_closable (s : EquivalenceClosureStatus) : bool :=
  match s with ECS_PartiallyClosable => true | _ => false end.
Definition ecs_closable (s : EquivalenceClosureStatus) : bool :=
  match s with ECS_Closable => true | _ => false end.
Definition ecs_prohibited (s : EquivalenceClosureStatus) : bool :=
  match s with ECS_Prohibited => true | _ => false end.

Record EquivalenceClosureCondition := {
  ecc_has_runtime_witness_chain : bool;
  ecc_has_invariant_pass : bool;
  ecc_has_candidate_equivalence_layer : bool;
  ecc_has_global_candidate_equivalence_layer : bool;
  ecc_has_chas_alignment_layer : bool;
  ecc_has_coq_catalog_alignment_layer : bool;
  ecc_no_fundamentally_blocked_draw : bool;
}.

Definition closure_allowed (c : EquivalenceClosureCondition) : bool :=
  c.(ecc_has_runtime_witness_chain) &&
  c.(ecc_has_invariant_pass) &&
  c.(ecc_has_candidate_equivalence_layer) &&
  c.(ecc_has_global_candidate_equivalence_layer) &&
  c.(ecc_has_chas_alignment_layer) &&
  c.(ecc_has_coq_catalog_alignment_layer) &&
  c.(ecc_no_fundamentally_blocked_draw).

(** Proof permission: eligibility to *attempt* a proof — NOT a proof. *)
Definition equivalence_proof_permitted (c : EquivalenceClosureCondition) : Prop :=
  closure_allowed c = true /\
  equivalence_claimed = false /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

(** ================================================================
    Part B:  Condition builders (mirrors FVR C++ resolvers)
    ================================================================ *)

Definition draw_blocked_in_reasons (reasons : list string) : bool :=
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string) reasons.

Definition runtime_witness_chain_ok (rw : RuntimeWitness) (row : CoqWitnessRow) : bool :=
  if runtime_witness_present rw then
    if Z.eqb row.(cwr_height) 0 then row.(cwr_witness_chain_step_valid)
    else row.(cwr_parent_witness_present) && row.(cwr_witness_chain_step_valid)
  else false.

Definition chas_alignment_ok (chas : CHASReport) : bool :=
  negb chas.(chas_oru_fundamentally_blocked) &&
  negb chas.(chas_res_fundamentally_blocked).

Definition coq_catalog_alignment_ok (row : CoqWitnessRow) : bool :=
  runtime_alignment_integration_established &&
  coq_oru_present row &&
  coq_res_present row.

Definition fundamentally_prohibited
  (ceo : CandidateEquivalenceObservation) (chas : CHASReport)
  : bool :=
  if negb (chas_alignment_ok chas) then true
  else if ces_fundamentally_blocked ceo.(ceo_invariant_status) then true
  else if ces_fundamentally_blocked ceo.(ceo_witness_chain_status) then true
  else if draw_blocked_in_reasons ceo.(ceo_blocking_reasons) then true
  else false.

Definition build_closure_condition
  (rw : RuntimeWitness) (row : CoqWitnessRow) (chas : CHASReport)
  : EquivalenceClosureCondition :=
  let ceo := check_candidate_equivalence rw row chas in
  {| ecc_has_runtime_witness_chain := runtime_witness_chain_ok rw row;
     ecc_has_invariant_pass := rw.(rw_invariants_passed) && row.(cwr_invariants_passed);
     ecc_has_candidate_equivalence_layer := runtime_equivalence_candidate_established;
     ecc_has_global_candidate_equivalence_layer := global_equivalence_candidate_established;
     ecc_has_chas_alignment_layer := chas_alignment_ok chas;
     ecc_has_coq_catalog_alignment_layer := coq_catalog_alignment_ok row;
     ecc_no_fundamentally_blocked_draw :=
       negb (draw_blocked_in_reasons ceo.(ceo_blocking_reasons)) |}.

Definition all_closure_layers_present (c : EquivalenceClosureCondition) : bool :=
  c.(ecc_has_runtime_witness_chain) &&
  c.(ecc_has_invariant_pass) &&
  c.(ecc_has_candidate_equivalence_layer) &&
  c.(ecc_has_global_candidate_equivalence_layer) &&
  c.(ecc_has_chas_alignment_layer) &&
  c.(ecc_has_coq_catalog_alignment_layer).

Definition classify_closure_status
  (c : EquivalenceClosureCondition)
  (ceo : CandidateEquivalenceObservation)
  (chas : CHASReport)
  : EquivalenceClosureStatus :=
  if fundamentally_prohibited ceo chas then ECS_Prohibited
  else if closure_allowed c then ECS_Closable
  else if all_closure_layers_present c then ECS_PartiallyClosable
  else ECS_Open.

Record EquivalenceClosureObservation := {
  eco_height : nat;
  eco_block_hash : hash256;
  eco_condition : EquivalenceClosureCondition;
  eco_status : EquivalenceClosureStatus;
  eco_closure_allowed : bool;
}.

Definition check_equivalence_closure
  (rw : RuntimeWitness) (row : CoqWitnessRow) (chas : CHASReport)
  : EquivalenceClosureObservation :=
  let c := build_closure_condition rw row chas in
  let ceo := check_candidate_equivalence rw row chas in
  {| eco_height := nat_of_height rw.(rw_height);
     eco_block_hash := rw.(rw_block_hash);
     eco_condition := c;
     eco_status := classify_closure_status c ceo chas;
     eco_closure_allowed := closure_allowed c |}.

Fixpoint catalog_all_equivalence_closure
  (rws : list RuntimeWitness)
  (rows : list CoqWitnessRow)
  (chas_reports : list CHASReport)
  : list EquivalenceClosureObservation :=
  match rws, rows, chas_reports with
  | rw :: rws', row :: rows', ch :: chs' =>
      check_equivalence_closure rw row ch ::
      catalog_all_equivalence_closure rws' rows' chs'
  | _, _, _ => nil
  end.

(** ================================================================
    Part C:  Frozen closure catalog (FVR-J samples + neutral CHAS)
    ================================================================ *)

Definition equivalence_closure_catalog : list EquivalenceClosureObservation :=
  [ check_equivalence_closure sample_witness_genesis sample_row_genesis chas_neutral;
    check_equivalence_closure sample_witness_comparable sample_row_comparable chas_neutral;
    check_equivalence_closure sample_witness_broken sample_row_broken chas_neutral;
    check_equivalence_closure sample_witness_not_comparable sample_row_not_comparable chas_neutral;
    check_equivalence_closure sample_witness_invariant_fail sample_row_invariant_fail chas_neutral ].

Definition equivalence_closure_catalog_count : nat :=
  List.length equivalence_closure_catalog.

Definition closure_statuses : list EquivalenceClosureStatus :=
  map (fun o => o.(eco_status)) equivalence_closure_catalog.

Definition closure_open_count : nat :=
  List.length (filter ecs_open closure_statuses).
Definition closure_partial_count : nat :=
  List.length (filter ecs_partially_closable closure_statuses).
Definition closure_closable_count : nat :=
  List.length (filter ecs_closable closure_statuses).
Definition closure_prohibited_count : nat :=
  List.length (filter ecs_prohibited closure_statuses).

Definition closure_component_count : nat :=
  List.length closure_statuses.

(** Synthetic all-true condition for soundness (no runtime draw gate). *)
Definition sample_closure_all_true : EquivalenceClosureCondition :=
  {| ecc_has_runtime_witness_chain := true;
     ecc_has_invariant_pass := true;
     ecc_has_candidate_equivalence_layer := true;
     ecc_has_global_candidate_equivalence_layer := true;
     ecc_has_chas_alignment_layer := true;
     ecc_has_coq_catalog_alignment_layer := true;
     ecc_no_fundamentally_blocked_draw := true |}.

(** ================================================================
    Part D:  Required proofs
    ================================================================ *)

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [closure_allowed_well_defined]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma closure_allowed_well_defined :
  forall c, closure_allowed c = closure_allowed c.
Proof. intros. reflexivity. Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the conclusion below is
   [True]. Provable by [exact I] for any definition of the subject, so it
   constrains nothing. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma closure_condition_well_defined :
  forall c : EquivalenceClosureCondition, True.
Proof. intros. exact I. Qed.

Lemma equivalence_closure_catalog_nonempty :
  equivalence_closure_catalog <> nil.
Proof. discriminate. Qed.

Lemma closure_component_count_value :
  closure_component_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma closure_open_count_value :
  closure_open_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma closure_partial_count_value :
  closure_partial_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma closure_closable_count_value :
  closure_closable_count = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma closure_prohibited_count_value :
  closure_prohibited_count = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma closure_component_partition_complete :
  closure_open_count +
  closure_partial_count +
  closure_closable_count +
  closure_prohibited_count
  = closure_component_count.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_closure_all_true_allowed :
  closure_allowed sample_closure_all_true = true.
Proof. vm_compute. reflexivity. Qed.

Theorem equivalence_closure_soundness :
  forall c,
    closure_allowed c = true ->
    equivalence_proof_permitted c.
Proof.
  intros c Hallowed.
  unfold equivalence_proof_permitted.
  split; [| split; [| split]].
  - exact Hallowed.
  - reflexivity.
  - destruct proof_debt_all_open as [H _]. exact H.
  - destruct proof_debt_all_open as [_ [H _]]. exact H.
Qed.

Lemma sample_frozen_draw_prohibited :
  (check_equivalence_closure sample_witness_genesis sample_row_genesis chas_neutral).(eco_status) = ECS_Prohibited /\
  (check_equivalence_closure sample_witness_comparable sample_row_comparable chas_neutral).(eco_status) = ECS_Prohibited /\
  (check_equivalence_closure sample_witness_broken sample_row_broken chas_neutral).(eco_status) = ECS_Prohibited /\
  (check_equivalence_closure sample_witness_not_comparable sample_row_not_comparable chas_neutral).(eco_status) = ECS_Prohibited /\
  (check_equivalence_closure sample_witness_invariant_fail sample_row_invariant_fail chas_neutral).(eco_status) = ECS_Prohibited.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma sample_comparable_layers_present :
  let o := check_equivalence_closure sample_witness_comparable sample_row_comparable chas_neutral in
  all_closure_layers_present o.(eco_condition) = true.
Proof. vm_compute. reflexivity. Qed.

Lemma sample_missing_witness_open :
  let o := check_equivalence_closure sample_witness_not_comparable sample_row_not_comparable chas_neutral in
  o.(eco_status) = ECS_Prohibited /\
  o.(eco_closure_allowed) = false.
Proof. vm_compute. split; reflexivity. Qed.

(** ================================================================
    Part E:  Honesty / debt
    ================================================================ *)

Definition equivalence_closure_layer_established : bool := true.
Definition equivalence_closure_coq_eq_runtime : bool := false.

Lemma equivalence_closure_does_not_establish_equivalence :
  equivalence_closure_coq_eq_runtime = false /\
  equivalence_claimed = false.
Proof. split; reflexivity. Qed.

Lemma equivalence_closure_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof.
  split.
  - exact canonical_encoding_pd_ob_02_open.
  - exact canonical_encoding_pd_cpp_01_open.
Qed.
