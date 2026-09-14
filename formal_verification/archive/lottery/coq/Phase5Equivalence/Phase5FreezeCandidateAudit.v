(** Phase5Equivalence/Phase5FreezeCandidateAudit.v
    Phase-5 mainnet freeze candidate audit (TASK-PROOF-029).

    Re-exports freeze candidate verdict, authority link, and snapshot
    alignment. No consensus / runtime / FVR modifications. *)

From Coq Require Import List Arith Bool Strings.String.
Import ListNotations.
Require Import Rigor.Phase5Equivalence.Phase5FreezeCandidateCatalog.
Require Import Rigor.Phase5Equivalence.Phase5FinalConsistencyCatalog.
Require Import Rigor.Phase5Equivalence.Phase5SummaryFinalCatalog.
Require Import Rigor.Phase5Equivalence.ProofObligationDependencyCatalog.
Require Import Rigor.Phase5Equivalence.PD4FOB02Readiness.
Require Import Rigor.Phase5Equivalence.PD4FCPP01ClosureModel.
Require Import Rigor.Phase5Equivalence.CandidateEquivalenceRuntimeModel.

Local Close Scope string_scope.

(** ---- Audit exports ---- *)

Definition audit_p5_freeze_candidate_established : bool :=
  p5_freeze_candidate_established.

Definition audit_p5_freeze_candidate_task : string :=
  p5_freeze_candidate_task.

Definition audit_p5_freeze_candidate_rpc_export : string :=
  p5_freeze_candidate_rpc_export.

(** ---- Snapshot re-exports ---- *)

Lemma audit_p5_freeze_candidate_global_consistency :
  phase5_global_consistency.
Proof. exact p5_freeze_candidate_inherits_global_consistency. Qed.

Lemma audit_p5_freeze_candidate_snapshot_verified :
  p5_freeze_candidate_pd13_closed = true /\
  p5_freeze_candidate_ob02_ready = true /\
  p5_freeze_candidate_cpp01_open = true /\
  p5_freeze_candidate_gv_total = 12%nat /\
  p5_freeze_candidate_gv_materialized = 9%nat.
Proof.
  repeat (apply conj).
  exact pd13_baseline_closed_true.
  exact p5_final_pd4f_ob02_ready_baseline.
  exact p5_final_pd4f_cpp01_open_baseline.
  reflexivity.
  reflexivity.
Qed.

Lemma audit_p5_freeze_candidate_no_equivalence :
  equivalence_claimed = false /\
  p5_freeze_candidate_coq_eq_runtime = false.
Proof.
  split; [| reflexivity].
  exact equivalence_not_claimed.
Qed.

Lemma audit_p5_freeze_candidate_draw_prohibited :
  p5_final_draw_permanently_non_closable = true.
Proof. exact draw_permanently_non_closable. Qed.

Lemma audit_p5_freeze_candidate_cpp01_stable :
  p5_final_cpp01_runtime_alignment = false /\
  p5_final_cpp01_candidate_equivalence = false /\
  p5_final_candidate_runtime_ready = false.
Proof.
  repeat (apply conj).
  exact cpp01_runtime_alignment_ready_baseline.
  exact cpp01_candidate_equivalence_ready_baseline.
  destruct candidate_equivalence_runtime_baseline_false as [H _]. exact H.
Qed.

Lemma audit_p5_freeze_candidate_freeze_link :
  String.eqb p5_freeze_candidate_freeze_link
    "MAINNET_PROTOCOL_FREEZE_DECLARATION.md/TASK-MAINNET-FREEZE-001"%string = true.
Proof. exact p5_final_links_freeze_protocol. Qed.

Lemma audit_p5_freeze_candidate_rpc_unchanged :
  String.eqb p5_freeze_candidate_rpc_export "getphase5summaryfinal"%string = true.
Proof. exact p5_freeze_candidate_rpc_unchanged. Qed.

(** ---- Master audit verdict ---- *)

Definition phase5_freeze_candidate_audit_verdict : Prop :=
  phase5_freeze_candidate_model_established /\
  audit_p5_freeze_candidate_established = true /\
  phase5_freeze_candidate_verdict /\
  phase5_global_consistency /\
  equivalence_claimed = false /\
  p5_final_draw_permanently_non_closable = true /\
  p5_freeze_candidate_coq_eq_runtime = false /\
  p5_final_cpp01_candidate_equivalence = false /\
  p5_final_candidate_runtime_ready = false /\
  (p5_freeze_candidate_ecs_open + p5_freeze_candidate_ecs_partial +
   p5_freeze_candidate_ecs_prohibited = p5_freeze_candidate_gv_total) /\
  String.eqb p5_freeze_candidate_freeze_link
    "MAINNET_PROTOCOL_FREEZE_DECLARATION.md/TASK-MAINNET-FREEZE-001"%string = true /\
  String.eqb audit_p5_freeze_candidate_rpc_export
    "getphase5summaryfinal"%string = true.

Lemma phase5_freeze_candidate_audit_verdict_holds :
  phase5_freeze_candidate_audit_verdict.
Proof.
  split. { exact phase5_freeze_candidate_model_established_holds. }
  split. { reflexivity. }
  split. { exact phase5_freeze_candidate_verdict_holds. }
  split. { exact phase5_global_consistency_holds. }
  split. { exact equivalence_not_claimed. }
  split. { exact draw_permanently_non_closable. }
  split. { reflexivity. }
  split. { exact cpp01_candidate_equivalence_ready_baseline. }
  split. { destruct candidate_equivalence_runtime_baseline_false as [H _]. exact H. }
  split. { exact p5_final_ecs_partition_complete. }
  split. { exact p5_final_links_freeze_protocol. }
  exact p5_freeze_candidate_rpc_unchanged.
Qed.
