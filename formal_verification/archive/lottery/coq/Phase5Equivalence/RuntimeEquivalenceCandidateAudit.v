(** Phase5Equivalence/RuntimeEquivalenceCandidateAudit.v
    Runtime equivalence candidate audit (TASK-FVR-J).

    Aggregates candidate-equivalence counts from
    RuntimeEquivalenceCandidateCatalog.v. Master verdict: no equivalence,
    proof debt OPEN. *)

From Coq Require Import List Bool ZArith Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.RuntimeEquivalenceCandidateCatalog.

(** ---- Totals ---- *)

Definition reca_field_count : nat := candidate_equivalence_field_count.
Definition reca_comparable : nat := candidate_equivalence_comparable_count.
Definition reca_partial : nat := candidate_equivalence_partial_count.
Definition reca_not_comparable : nat := candidate_equivalence_not_comparable_count.
Definition reca_blocked : nat := candidate_equivalence_blocked_count.

Lemma reca_field_count_value : reca_field_count = 20%nat.
Proof. exact candidate_equivalence_field_count_value. Qed.

Lemma reca_comparable_value : reca_comparable = 8%nat.
Proof. exact candidate_equivalence_comparable_count_value. Qed.

Lemma reca_partial_value : reca_partial = 5%nat.
Proof. exact candidate_equivalence_partial_count_value. Qed.

Lemma reca_not_comparable_value : reca_not_comparable = 3%nat.
Proof. exact candidate_equivalence_not_comparable_count_value. Qed.

Lemma reca_blocked_value : reca_blocked = 4%nat.
Proof. exact candidate_equivalence_blocked_count_value. Qed.

Lemma reca_partition_complete :
  reca_comparable + reca_partial + reca_not_comparable + reca_blocked = reca_field_count.
Proof. exact candidate_equivalence_partition_complete. Qed.

(** ---- Re-exported obligations ---- *)

Lemma audit_runtime_equivalence_candidate_catalog_nonempty :
  runtime_equivalence_candidate_catalog <> nil.
Proof. exact runtime_equivalence_candidate_catalog_nonempty. Qed.

(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): the statement below has the
   form [X = X]. It is trivially true by reflexivity and constrains nothing
   about [audit_check_candidate_equivalence_deterministic]. Retained for catalog shape only. NOT EVIDENCE. *)
Lemma audit_check_candidate_equivalence_deterministic :
  forall rw row chas,
    check_candidate_equivalence rw row chas =
    check_candidate_equivalence rw row chas.
Proof. exact check_candidate_equivalence_deterministic. Qed.

Lemma audit_check_candidate_equivalence_total :
  forall rw row chas, exists obs, obs = check_candidate_equivalence rw row chas.
Proof. exact check_candidate_equivalence_total. Qed.

Lemma audit_candidate_equivalence_status_well_defined :
  forall s, In s candidate_equivalence_statuses ->
    match s with
    | CES_Comparable => True
    | CES_PartiallyComparable => True
    | CES_NotComparable => True
    | CES_FundamentallyBlocked => True
    end.
Proof. exact candidate_equivalence_status_well_defined. Qed.

Lemma audit_runtime_equivalence_candidate_does_not_establish_equivalence :
  runtime_equivalence_candidate_coq_eq_runtime = false /\ equivalence_claimed = false.
Proof. exact runtime_equivalence_candidate_does_not_establish_equivalence. Qed.

Lemma audit_runtime_equivalence_candidate_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact runtime_equivalence_candidate_preserves_open_proof_debt. Qed.

Lemma audit_runtime_equivalence_candidate_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.

Lemma audit_sample_draw_blocked_on_frozen_catalog :
  existsb (String.eqb "DRAW-CES-FundamentallyBlocked"%string)
    (check_candidate_equivalence sample_witness_genesis sample_row_genesis chas_neutral).(ceo_blocking_reasons) = true.
Proof.
  destruct sample_draw_blocked_on_frozen_catalog as [H _].
  exact H.
Qed.

(** ---- Master audit verdict ---- *)

Definition runtime_equivalence_candidate_audit_verdict : Prop :=
  reca_field_count = 20%nat /\
  reca_comparable = 8%nat /\
  reca_partial = 5%nat /\
  reca_not_comparable = 3%nat /\
  reca_blocked = 4%nat /\
  reca_comparable + reca_partial + reca_not_comparable + reca_blocked = reca_field_count /\
  runtime_equivalence_candidate_established = true /\
  runtime_equivalence_candidate_coq_eq_runtime = false /\
  equivalence_claimed = false /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma runtime_equivalence_candidate_audit_verdict_holds :
  runtime_equivalence_candidate_audit_verdict.
Proof.
  unfold runtime_equivalence_candidate_audit_verdict.
  repeat split; try reflexivity.
  all: try exact reca_partition_complete.
  all: try exact equivalence_not_claimed.
  all: try (destruct proof_debt_all_open as [H _]; exact H).
  all: try (destruct proof_debt_all_open as [_ [H _]]; exact H).
Qed.

(** ---- Phase-D golden vector closure alignment (TASK-PROOF-018) ---- *)

Definition reca_gv_count : nat := golden_vector_equivalence_candidate_count.
Definition reca_gv_open : nat := golden_vector_closure_open_count.
Definition reca_gv_partial : nat := golden_vector_closure_partial_count.
Definition reca_gv_prohibited : nat := golden_vector_closure_prohibited_count.

Lemma reca_gv_count_value : reca_gv_count = 12%nat.
Proof. exact golden_vector_equivalence_candidate_count_value. Qed.

Lemma reca_gv_open_value : reca_gv_open = 3%nat.
Proof. exact golden_vector_closure_open_count_value. Qed.

Lemma reca_gv_partial_value : reca_gv_partial = 7%nat.
Proof. exact golden_vector_closure_partial_count_value. Qed.

Lemma reca_gv_prohibited_value : reca_gv_prohibited = 2%nat.
Proof. exact golden_vector_closure_prohibited_count_value. Qed.

Lemma reca_gv_partition_complete :
  reca_gv_open + reca_gv_partial + reca_gv_prohibited = reca_gv_count.
Proof. exact golden_vector_closure_partition_complete. Qed.

Lemma audit_golden_vector_draw_closure_allowed_false :
  golden_vector_closure_allowed gv_snap_draw_0001 = false /\
  golden_vector_closure_allowed gv_snap_draw_0002 = false.
Proof. exact golden_vector_draw_closure_allowed_false. Qed.

Lemma audit_golden_vector_draw_closure_prohibited :
  classify_golden_vector_closure_gate gv_snap_draw_0001 = GVCG_ECS_Prohibited /\
  classify_golden_vector_closure_gate gv_snap_draw_0002 = GVCG_ECS_Prohibited.
Proof. exact golden_vector_draw_closure_prohibited. Qed.

Lemma audit_golden_vector_ai_partial_closure_gate :
  classify_golden_vector_closure_gate gv_snap_oru_0001 = GVCG_ECS_PartiallyClosable /\
  classify_golden_vector_closure_gate gv_snap_state_0003 = GVCG_ECS_PartiallyClosable.
Proof. exact golden_vector_ai_partial_implies_partially_closable. Qed.

Lemma audit_golden_vector_equivalence_claimed_false :
  forall snap,
    (build_golden_vector_equivalence_obs snap).(gveco_equivalence_claimed) = false /\
    (build_golden_vector_equivalence_obs snap).(gveco_coq_equiv_runtime) = coq_equiv_runtime_not_established.
Proof. exact golden_vector_equivalence_claimed_false. Qed.

Lemma audit_golden_vector_ecs_alignment_holds :
  reca_gv_count = 12%nat /\
  reca_gv_open = 3%nat /\
  reca_gv_partial = 7%nat /\
  reca_gv_prohibited = 2%nat /\
  reca_gv_open + reca_gv_partial + reca_gv_prohibited = reca_gv_count /\
  phase_d_closure_alignment_established = true /\
  equivalence_claimed = false.
Proof. repeat split; reflexivity. Qed.

Definition phase_d_closure_alignment_verdict : Prop :=
  reca_gv_count = 12%nat /\
  reca_gv_open = 3%nat /\
  reca_gv_partial = 7%nat /\
  reca_gv_prohibited = 2%nat /\
  reca_gv_open + reca_gv_partial + reca_gv_prohibited = reca_gv_count /\
  phase_d_closure_alignment_established = true /\
  runtime_equivalence_candidate_coq_eq_runtime = false /\
  equivalence_claimed = false /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma phase_d_closure_alignment_verdict_holds :
  phase_d_closure_alignment_verdict.
Proof. unfold phase_d_closure_alignment_verdict. repeat split; reflexivity. Qed.
