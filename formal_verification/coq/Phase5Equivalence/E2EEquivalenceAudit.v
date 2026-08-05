(** Phase5Equivalence/E2EEquivalenceAudit.v
    End-to-end equivalence audit (TASK-018FF-G).

    Master totals over the 6 × 3 matrix from E2EEquivalenceCatalog.v.
    Records circularity, partials, and proof debts that remain open.
    No equivalence is asserted; readiness is PARTIAL. *)

From Coq Require Import List Bool Arith.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.EquivalenceFramework.
Require Import Rigor.Phase5Equivalence.DrawEquivalence.
Require Import Rigor.Phase5Equivalence.DrawEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ReplayEquivalence.
Require Import Rigor.Phase5Equivalence.ReplayEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ReorgEquivalence.
Require Import Rigor.Phase5Equivalence.ReorgEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalence.
Require Import Rigor.Phase5Equivalence.ConnectBlockEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalence.
Require Import Rigor.Phase5Equivalence.SupplyEligibilityEquivalenceAudit.
Require Import Rigor.Phase5Equivalence.E2EEquivalenceCatalog.

(** ---- Re-exported counts ---- *)

Definition e2e_audit_total : nat := e2e_total_cells.
Definition e2e_audit_agree : nat := e2e_agree_count.
Definition e2e_audit_disagree : nat := e2e_disagree_count.
Definition e2e_audit_missing : nat := e2e_missing_count.
Definition e2e_audit_partial : nat := e2e_partial_count.

Lemma e2e_audit_total_value : e2e_audit_total = 18%nat.
Proof. exact e2e_total_cells_value. Qed.

Lemma e2e_audit_agree_value : e2e_audit_agree = 5%nat.
Proof. exact e2e_agree_count_value. Qed.

Lemma e2e_audit_disagree_value : e2e_audit_disagree = 8%nat.
Proof. exact e2e_disagree_count_value. Qed.

Lemma e2e_audit_missing_value : e2e_audit_missing = 4%nat.
Proof. exact e2e_missing_count_value. Qed.

Lemma e2e_audit_partial_value : e2e_audit_partial = 1%nat.
Proof. exact e2e_partial_count_value. Qed.

Lemma e2e_audit_counts_partition :
  e2e_audit_agree + e2e_audit_disagree + e2e_audit_missing + e2e_audit_partial
  = e2e_audit_total.
Proof. exact e2e_counts_partition. Qed.

(** ---- Honesty: no full agreement ---- *)

Lemma e2e_audit_no_full_agreement :
  e2e_audit_agree < e2e_audit_total.
Proof.
  rewrite e2e_audit_agree_value, e2e_audit_total_value.
  repeat constructor.
Qed.

Lemma e2e_audit_disagreement_present :
  0 < e2e_audit_disagree.
Proof.
  rewrite e2e_audit_disagree_value. repeat constructor.
Qed.

Lemma e2e_audit_missing_present :
  0 < e2e_audit_missing.
Proof.
  rewrite e2e_audit_missing_value. repeat constructor.
Qed.

(** ---- Per-component PARTIAL verdicts (re-exports from 018FF-B..F) ---- *)

Lemma e2e_per_component_partial :
  draw_equivalence_marked_partial = true /\
  replay_equivalence_marked_partial = true /\
  reorg_equivalence_marked_partial = true /\
  cb_equivalence_marked_partial = true /\
  se_equivalence_marked_partial = true.
Proof.
  split; [exact draw_equivalence_marked_partial_value | ].
  split; [exact replay_equivalence_marked_partial_value | ].
  split; [exact reorg_equivalence_marked_partial_value | ].
  split; [exact cb_equivalence_marked_partial_value | ].
  exact se_equivalence_marked_partial_value.
Qed.

(** ---- Circularity tally (frozen 015DD taxonomy) ---- *)

Lemma e2e_audit_circularity_tally :
  e2e_imported_eligibility_count = 4%nat /\
  e2e_payload_domain_gap_count = 8%nat /\
  e2e_supply_absent_count = 2%nat.
Proof.
  split; [exact e2e_imported_eligibility_count_value | ].
  split; [exact e2e_payload_domain_gap_count_value | ].
  exact e2e_supply_absent_count_value.
Qed.

(** ---- Proof debts remain open (mirrors 017FF flags) ---- *)

Lemma e2e_audit_proof_debts_remain_open :
  negb proof_debt_placeholder.(pdp_pd_4f_ob_02_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_cpp_01_closed) = true /\
  negb proof_debt_placeholder.(pdp_pd_4f_ob_03_closed) = true.
Proof. exact proof_debt_all_open. Qed.

Lemma e2e_audit_eligibility_imported :
  negb eligibility_rederived_in_coq = true.
Proof. exact eligibility_not_rederived. Qed.

Lemma e2e_audit_frozen_oracle_not_wired :
  negb frozen_cpp_oracle_wired = true.
Proof. exact frozen_cpp_oracle_not_wired. Qed.

Lemma e2e_audit_raw_frozen_comparison_not_performed :
  negb raw_frozen_comparison_performed = true.
Proof. exact raw_frozen_comparison_not_performed. Qed.

(** ---- Readiness predicate (PARTIAL only) ---- *)

Definition e2e_readiness_partial : bool :=
  Nat.eqb e2e_audit_total 18 &&
  Nat.eqb e2e_audit_agree 5 &&
  Nat.eqb e2e_audit_disagree 8 &&
  Nat.eqb e2e_audit_missing 4 &&
  Nat.eqb e2e_audit_partial 1 &&
  negb equivalence_claimed.

Lemma e2e_readiness_partial_value : e2e_readiness_partial = true.
Proof. vm_compute. reflexivity. Qed.

(** ---- MASTER VERDICT ---- *)

Definition e2e_master_verdict : Prop :=
  e2e_audit_total = 18%nat /\
  e2e_audit_agree = 5%nat /\
  e2e_audit_disagree = 8%nat /\
  e2e_audit_missing = 4%nat /\
  e2e_audit_partial = 1%nat /\
  e2e_audit_agree + e2e_audit_disagree + e2e_audit_missing + e2e_audit_partial
    = e2e_audit_total /\
  e2e_readiness_partial = true /\
  equivalence_claimed = false.

Lemma e2e_master_verdict_holds : e2e_master_verdict.
Proof.
  unfold e2e_master_verdict.
  split; [exact e2e_audit_total_value | ].
  split; [exact e2e_audit_agree_value | ].
  split; [exact e2e_audit_disagree_value | ].
  split; [exact e2e_audit_missing_value | ].
  split; [exact e2e_audit_partial_value | ].
  split; [exact e2e_audit_counts_partition | ].
  split; [exact e2e_readiness_partial_value | ].
  reflexivity.
Qed.

(** Explicit non-equivalence invariant. *)
Lemma e2e_audit_no_global_equivalence : equivalence_claimed = false.
Proof. reflexivity. Qed.

Lemma e2e_audit_no_equivalence_claimed :
  negb equivalence_claimed = true.
Proof. exact equivalence_not_claimed. Qed.
