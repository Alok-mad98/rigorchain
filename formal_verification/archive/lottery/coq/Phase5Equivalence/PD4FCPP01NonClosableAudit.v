(** Phase5Equivalence/PD4FCPP01NonClosableAudit.v
    PD-4F-CPP-01 non-closability audit (TASK-PROOF-033).

    Re-exports closure-path analysis theorems and frozen baseline verdict.
    No consensus / runtime / FVR modifications. *)

From Coq Require Import List Arith Bool Strings.String.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.PD4FCPP01NonClosableCatalog.
Require Import Rigor.Phase5Equivalence.Phase5TerminalStatusCatalog.
Require Import Rigor.Phase5Equivalence.PD4FCPP01ClosureModel.
Require Import Rigor.Phase5Equivalence.Phase5SummaryFinalCatalog.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition audit_cpp01_nc_analysis_established : bool :=
  cpp01_nc_analysis_established.

Definition audit_cpp01_structural_path_exists : bool :=
  cpp01_structural_closure_path_exists.

Definition audit_cpp01_legal_path_at_baseline : bool :=
  cpp01_legal_closure_path_at_baseline.

(** ---- Theorem re-exports ---- *)

Lemma audit_cpp01_no_legal_closure_path :
  pd4f_cpp01_closable_frozen = false.
Proof. exact pd4f_cpp01_closable_frozen_false. Qed.

Lemma audit_draw_prohibition_blocks_global_equivalence :
  p5_final_draw_permanently_non_closable = true ->
  equivalence_claimed = false.
Proof.
  intros Hdraw.
  exact (draw_prohibition_blocks_global_equivalence Hdraw eq_refl).
Qed.

Lemma audit_spec007_blocks_equivalence_claim :
  equivalence_claimed = false /\
  cpp01_spec007_closure_gate_satisfied = false.
Proof.
  split; [| reflexivity].
  exact equivalence_not_claimed.
Qed.

Lemma audit_cpp01_permanently_non_closable :
  forall catalog, pd4f_cpp01_closable catalog = false.
Proof.
  intros catalog.
  destruct cpp01_all_gates_frozen_false_holds as [Hrt [Hvec [Hcoq Hcand]]].
  exact (pd4f_cpp01_not_closable_when_gates_closed Hrt Hvec Hcoq Hcand catalog).
Qed.

Lemma audit_phase5_terminal_implies_cpp01_non_closable :
  phase5_terminal_status ->
  pd4f_cpp01_closable_frozen = false.
Proof. exact phase5_terminal_status_implies_cpp01_non_closable. Qed.

Lemma audit_cpp01_closure_graph_established :
  List.length cpp01_nc_readiness_graph = 5%nat /\
  List.length cpp01_nc_blocked_graph = 7%nat.
Proof.
  destruct cpp01_nc_graph_counts as [_ [Hr [_ Hb]]].
  split; assumption.
Qed.

(** ---- Master audit verdict ---- *)

Definition pd4f_cpp01_non_closability_audit_verdict : Prop :=
  pd4f_cpp01_non_closability_model_established /\
  audit_cpp01_nc_analysis_established = true /\
  pd4f_cpp01_non_closability_verdict /\
  audit_cpp01_structural_path_exists = true /\
  audit_cpp01_legal_path_at_baseline = false /\
  (forall catalog, pd4f_cpp01_closable catalog = false) /\
  equivalence_claimed = false /\
  p5_final_draw_permanently_non_closable = true /\
  phase5_terminal_status.

Lemma pd4f_cpp01_non_closability_audit_verdict_holds :
  pd4f_cpp01_non_closability_audit_verdict.
Proof.
  split. { exact pd4f_cpp01_non_closability_model_established_holds. }
  split. { reflexivity. }
  split. { exact pd4f_cpp01_non_closability_verdict_holds. }
  split. { reflexivity. }
  split. { unfold cpp01_legal_closure_path_at_baseline. exact pd4f_cpp01_closable_frozen_false. }
  split. { intros catalog. destruct cpp01_all_gates_frozen_false_holds as [Hrt [Hvec [Hcoq Hcand]]].
    exact (pd4f_cpp01_not_closable_when_gates_closed Hrt Hvec Hcoq Hcand catalog). }
  split. { exact equivalence_not_claimed. }
  split. { exact draw_permanently_non_closable. }
  exact phase5_terminal_status_holds.
Qed.
