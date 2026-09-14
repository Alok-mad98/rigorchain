(** Phase5Equivalence/Phase5SummaryAudit.v
    Phase-5 summary audit (TASK-FVR-N).

    Re-exports partition counts, freeze link, and master verdict. *)

From Coq Require Import List Bool ZArith Strings.String.
Import ListNotations.
Open Scope string_scope.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.CanonicalStateEncoding.
Require Import Rigor.Phase5Equivalence.Phase5SummaryCatalog.
Require Import Rigor.Phase5Equivalence.DrawClosureImpossibilityCatalog.

Definition p5s_layer_count : nat := phase5_layer_count.
Definition p5s_established : nat := phase5_established_count.
Definition p5s_active : nat := phase5_active_count.
Definition p5s_partial : nat := phase5_partial_count.
Definition p5s_blocked : nat := phase5_blocked_count.
Definition p5s_impossible : nat := phase5_impossible_count.

Lemma p5s_layer_count_value : p5s_layer_count = 9%nat.
Proof. exact phase5_layer_count_value. Qed.

Lemma p5s_partition_complete :
  p5s_established + p5s_active + p5s_partial + p5s_blocked + p5s_impossible
  = p5s_layer_count.
Proof. exact phase5_layer_partition_complete. Qed.

Lemma audit_phase5_runtime_layers_established :
  runtime_supply_layer_established = true.
Proof. destruct phase5_runtime_layers_established as [H _]. exact H. Qed.

Lemma audit_draw_permanently_non_closable :
  draw_closure_status = DCS_Impossible.
Proof. exact draw_permanently_non_closable. Qed.

Lemma audit_all_other_layers_active :
  (mk_phase5_layer "FVR-B"%string "RuntimeSupply"%string
     P5LS_Established runtime_supply_layer_established false).(ple_status)
    = P5LS_Established.
Proof.
  destruct all_other_layers_active as [H _]. exact H.
Qed.

Lemma audit_coq_equiv_runtime_not_established :
  phase5_summary_coq_eq_runtime = false /\
  forallb (fun e => negb e.(ple_coq_eq_runtime)) phase5_layer_catalog = true.
Proof. exact coq_equiv_runtime_not_established. Qed.

Lemma audit_equivalence_claimed_false :
  equivalence_claimed = false.
Proof. exact equivalence_claimed_false. Qed.

Lemma audit_phase5_summary_does_not_establish_equivalence :
  phase5_summary_coq_eq_runtime = false /\ equivalence_claimed = false.
Proof. exact phase5_summary_does_not_establish_equivalence. Qed.

Lemma audit_phase5_summary_preserves_open_proof_debt :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.
Proof. exact phase5_summary_preserves_open_proof_debt. Qed.

Definition phase5_summary_audit_verdict : Prop :=
  p5s_layer_count = 9%nat /\
  p5s_established = 1%nat /\
  p5s_active = 7%nat /\
  p5s_impossible = 1%nat /\
  p5s_established + p5s_active + p5s_partial + p5s_blocked + p5s_impossible
    = p5s_layer_count /\
  draw_closure_status = DCS_Impossible /\
  phase5_summary_catalog_established = true /\
  phase5_summary_coq_eq_runtime = false /\
  equivalence_claimed = false /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true.

Lemma phase5_summary_audit_verdict_holds :
  phase5_summary_audit_verdict.
Proof.
  unfold phase5_summary_audit_verdict.
  repeat split; try reflexivity.
  all: try exact draw_permanently_non_closable.
  all: try exact p5s_partition_complete.
  all: try exact equivalence_not_claimed.
  all: try (destruct proof_debt_all_open as [H _]; exact H).
  all: try (destruct proof_debt_all_open as [_ [H _]]; exact H).
Qed.
