(** Phase4GRefinement/FinalReadinessAudit.v
    Final readiness audit (TASK-016EE).

    FREEZE/RE-EXPORT layer. Generates final tabular summaries and master Qed
    invariants over the 015DD closure + 016EE final verdict. No new claims —
    every Qed below is discharged by an existing 015DD/016EE-Catalog/Verdict
    lemma. Honesty boundary preserved: PARTIAL verdict, NO equivalence claim.

    AUDIT / SPEC ONLY. No consensus / runtime / RFC / SHA256 / ConvergeRoot /
    ConnectBlock / Replay / Reorg / mining code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.
Require Import Rigor.Phase4GRefinement.EndToEndClosureCatalog.
Require Import Rigor.Phase4GRefinement.EndToEndClosureAudit.
Require Import Rigor.Phase4GRefinement.FinalReadinessCatalog.
Require Import Rigor.Phase4GRefinement.FinalReadinessVerdict.

(** ================================================================
    Part A:  Per-component frozen tables
    ================================================================ *)

(** A consolidated per-component row: projection status + circular kind. *)
Record FinalComponentRow := {
  fcr_component : BridgeComponent;
  fcr_projection : ProjectionStatus;
  fcr_circular : CircularDependencyKind;
}.

Definition mk_component_row (c : BridgeComponent) : FinalComponentRow :=
  {| fcr_component := c;
     fcr_projection := final_status c;
     fcr_circular := final_circular c |}.

Definition final_component_table : list FinalComponentRow :=
  map mk_component_row closure_components.

Lemma final_component_table_length :
  length final_component_table = 5%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma final_component_table_components :
  map fcr_component final_component_table
    = [ BC_Draw; BC_Replay; BC_Reorg; BC_ConnectBlock; BC_Supply ].
Proof. vm_compute. reflexivity. Qed.

(** Per-component status check: matches the Final verdict statuses. *)
Lemma final_component_table_statuses :
  map fcr_projection final_component_table
    = [ PS_Defined; PS_Partial; PS_Partial; PS_Partial; PS_Partial ].
Proof. vm_compute. reflexivity. Qed.

(** Per-component circular check: matches the Final verdict circular kinds. *)
Lemma final_component_table_circular :
  map fcr_circular final_component_table
    = [ CDK_ImportedEligibility;
        CDK_PayloadDomainGap;
        CDK_PayloadDomainGap;
        CDK_PayloadDomainGap;
        CDK_SupplyAbsent ].
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part B:  Source × component matrix counts
    ================================================================ *)

Definition source_eqb (a b : ClosureSource) : bool :=
  match a, b with
  | CS_Frozen, CS_Frozen => true
  | CS_Runtime, CS_Runtime => true
  | CS_Semantic, CS_Semantic => true
  | _, _ => false
  end.

Definition source_count (s : ClosureSource) : nat :=
  length (filter (fun e => source_eqb e.(fe_source) s) final_catalog).

Definition frozen_count : nat := source_count CS_Frozen.
Definition runtime_count : nat := source_count CS_Runtime.
Definition semantic_count : nat := source_count CS_Semantic.

Lemma source_count_balanced :
  frozen_count = 5%nat /\ runtime_count = 5%nat /\ semantic_count = 5%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma source_counts_sum_to_total :
  frozen_count + runtime_count + semantic_count = total_entry_count.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part C:  Verdict tables (Agreement / Disagreement / Missing)
    ================================================================ *)

Record FinalVerdictRow := {
  fvr_label : nat;            (* 0=Agreement, 1=Disagreement, 2=Missing *)
  fvr_count : nat;
}.

Definition final_verdict_table : list FinalVerdictRow :=
  [ {| fvr_label := 0; fvr_count := final_total_agreement |};
    {| fvr_label := 1; fvr_count := final_total_disagreement |};
    {| fvr_label := 2; fvr_count := final_total_missing |} ].

Lemma final_verdict_table_length : length final_verdict_table = 3%nat.
Proof. reflexivity. Qed.

Lemma final_verdict_table_counts :
  map fvr_count final_verdict_table = [ 0%nat; 8%nat; 2%nat ].
Proof.
  unfold final_verdict_table. simpl.
  rewrite final_total_agreement_value.
  rewrite final_total_disagreement_value.
  rewrite final_total_missing_value.
  reflexivity.
Qed.

(** ================================================================
    Part D:  Projection-status tables
    ================================================================ *)

Record FinalProjectionRow := {
  fpr_label : ProjectionStatus;
  fpr_count : nat;
}.

Definition final_projection_table : list FinalProjectionRow :=
  [ {| fpr_label := PS_Defined; fpr_count := final_defined_count |};
    {| fpr_label := PS_Partial; fpr_count := final_partial_count |};
    {| fpr_label := PS_Undefined; fpr_count := final_undefined_count |} ].

Lemma final_projection_table_length : length final_projection_table = 3%nat.
Proof. reflexivity. Qed.

Lemma final_projection_table_counts :
  map fpr_count final_projection_table = [ 2%nat; 6%nat; 0%nat ].
Proof.
  unfold final_projection_table. simpl.
  rewrite final_defined_count_value.
  rewrite final_partial_count_value.
  rewrite final_undefined_count_value.
  reflexivity.
Qed.

(** ================================================================
    Part E:  Remaining proof-debt table
    ================================================================ *)

Inductive ProofDebtId := PDI_OB_02 | PDI_CPP_01 | PDI_OB_03.

Record FinalProofDebtRow := {
  fpdr_id : ProofDebtId;
  fpdr_closed : bool;
}.

Definition final_proof_debt_table : list FinalProofDebtRow :=
  [ {| fpdr_id := PDI_OB_02; fpdr_closed := final_proof_debt.(fpd_pd_4f_ob_02) |};
    {| fpdr_id := PDI_CPP_01; fpdr_closed := final_proof_debt.(fpd_pd_4f_cpp_01) |};
    {| fpdr_id := PDI_OB_03; fpdr_closed := final_proof_debt.(fpd_pd_4f_ob_03) |} ].

Lemma final_proof_debt_table_length : length final_proof_debt_table = 3%nat.
Proof. reflexivity. Qed.

(** All three remaining debts are recorded as still OPEN/PARTIAL (closed=false). *)
Lemma final_proof_debt_all_open :
  map fpdr_closed final_proof_debt_table = [ false; false; false ].
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part F:  Master final-readiness invariants
    ================================================================ *)

(** Master no-equivalence invariant (re-export). *)
Lemma final_audit_no_equivalence :
  final_total_agreement < final_verdict_total.
Proof. exact final_no_equivalence. Qed.

(** Master Qed: every required final-readiness claim, in one place. *)
Lemma final_audit_summary :
  final_total_agreement = 0%nat /\
  final_total_disagreement = 8%nat /\
  final_total_missing = 2%nat /\
  final_verdict_total = 10%nat /\
  final_defined_count = 2%nat /\
  final_partial_count = 6%nat /\
  final_undefined_count = 0%nat /\
  total_entry_count = 15%nat /\
  final_status BC_Supply = PS_Partial /\
  final_eligibility_status = PS_Partial /\
  final_proof_debt.(fpd_pd_4f_ob_02) = false /\
  final_proof_debt.(fpd_pd_4f_cpp_01) = false /\
  final_proof_debt.(fpd_pd_4f_ob_03) = false /\
  final_verdict = RV_Partial.
Proof.
  split. exact final_total_agreement_value.
  split. exact final_total_disagreement_value.
  split. exact final_total_missing_value.
  split. exact final_verdict_total_value.
  split. exact final_defined_count_value.
  split. exact final_partial_count_value.
  split. exact final_undefined_count_value.
  split. exact total_entry_count_value.
  split. reflexivity.
  split. exact final_eligibility_status_value.
  split. reflexivity.
  split. reflexivity.
  split. reflexivity.
  reflexivity.
Qed.

(** Final consolidated circular-dependency table for the five consensus
    components, re-exported from 015DD `consensus_components_have_residual`. *)
Lemma final_audit_circular_table :
  final_circular BC_Draw = CDK_ImportedEligibility /\
  final_circular BC_Replay = CDK_PayloadDomainGap /\
  final_circular BC_Reorg = CDK_PayloadDomainGap /\
  final_circular BC_ConnectBlock = CDK_PayloadDomainGap /\
  final_circular BC_Supply = CDK_SupplyAbsent.
Proof. exact final_circular_consensus_components_recorded. Qed.

(** Final verdict is PARTIAL — explicit, machine-verifiable. *)
Lemma final_audit_verdict_partial : final_verdict = RV_Partial.
Proof. exact final_verdict_is_partial. Qed.

Lemma final_audit_verdict_not_equivalent : final_verdict <> RV_Equivalent.
Proof. exact final_verdict_not_equivalent. Qed.
