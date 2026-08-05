(** Phase4GRefinement/EndToEndClosureAudit.v
    End-to-end closure audit (TASK-015DD).

    Final consolidation of the Phase-4F / Phase-4G end-to-end readiness with the
    014CC Supply + Eligibility/σ projections INTEGRATED. Recomputes the
    agreement / disagreement / missing counts from the 014CC pair lists, tallies
    the gap-derived projection catalog (Defined / Partial / Undefined), records
    the per-component closure projection status and residual circular
    dependencies, and proves the remaining proof debt is NOT closed.

    AUDIT / SPEC ONLY. Does NOT claim Coq = Runtime / Coq = Frozen C++ / Coq =
    Consensus. No consensus / runtime / RFC / SHA256 / ConvergeRoot / ConnectBlock
    / Replay / Reorg / mining code is modified. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.ConsensusOracleBridge.
Require Import Rigor.Phase4FConsensus.FrozenCppAgreement.
Require Import Rigor.Phase4FConsensus.SemanticVsFrozenComparison.
Require Import Rigor.Phase4FConsensus.SemanticVsLiveRuntimeComparison.
Require Import Rigor.Phase4GRefinement.RefinementProjection.
Require Import Rigor.Phase4GRefinement.RefinementProjectionCatalog.
Require Import Rigor.Phase4GRefinement.RefinementProjectionAudit.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityProjection.
Require Import Rigor.Phase4GRefinement.SupplyEligibilityCatalog.
Require Import Rigor.Phase4GRefinement.EndToEndClosureCatalog.

(** ================================================================
    Part A:  Recomputed agreement / disagreement / missing counts
    ================================================================ *)

(** Per-source raw payload-comparison results over the 014CC pair lists
    (Supply now has a projected Coq observation, but frozen/runtime remain
    absent, so its result is Missing). *)
Definition closure_sf_results : list FrozenAgreementResult :=
  semantic_frozen_results supply_eligibility_semantic_vs_frozen_pairs.
Definition closure_sr_results : list FrozenAgreementResult :=
  semantic_runtime_results supply_eligibility_semantic_vs_runtime_pairs.

Definition closure_all_results : list FrozenAgreementResult :=
  closure_sf_results ++ closure_sr_results.

Definition e2e_total_agreement : nat :=
  length (filter far_is_agree closure_all_results).
Definition e2e_total_disagreement : nat :=
  length (filter far_is_disagree closure_all_results).
Definition e2e_total_missing : nat :=
  length (filter far_is_missing closure_all_results).

(** REQUIRED: the headline end-to-end counts (unchanged from 014CC / 008V). *)
Lemma e2e_total_agreement_value : e2e_total_agreement = 0%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_total_disagreement_value : e2e_total_disagreement = 8%nat.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_total_missing_value : e2e_total_missing = 2%nat.
Proof. vm_compute. reflexivity. Qed.

(** The three tallies partition all verdicts (10 = 5 components x 2 sources). *)
Lemma e2e_counts_partition :
  e2e_total_agreement + e2e_total_disagreement + e2e_total_missing
    = length closure_all_results.
Proof. vm_compute. reflexivity. Qed.

Lemma e2e_verdict_total_value : length closure_all_results = 10%nat.
Proof. vm_compute. reflexivity. Qed.

(** ================================================================
    Part B:  Projection counts consistent with catalog entries
    ================================================================ *)

(** Gap-derived projection counts (006S/007U): the 8 disagreements map to
    2 Defined (Draw x {frozen, runtime}) + 6 Partial (Replay/Reorg/ConnectBlock
    x {frozen, runtime}) + 0 Undefined. These are re-exported here as the
    closure projection counts. *)
Definition closure_defined_count : nat := defined_projection_count.
Definition closure_partial_count : nat := partial_projection_count.
Definition closure_undefined_count : nat := undefined_projection_count.

Lemma closure_defined_count_value : closure_defined_count = 2%nat.
Proof. exact defined_projection_count_value. Qed.

Lemma closure_partial_count_value : closure_partial_count = 6%nat.
Proof. exact partial_projection_count_value. Qed.

Lemma closure_undefined_count_value : closure_undefined_count = 0%nat.
Proof. exact undefined_projection_count_value. Qed.

(** Projection counts are consistent with the gap catalog size (8 entries). *)
Lemma closure_projection_counts_consistent :
  closure_defined_count + closure_partial_count + closure_undefined_count
    = projection_count.
Proof. exact projection_statistics_exist. Qed.

Lemma closure_projection_count_is_eight :
  closure_defined_count + closure_partial_count + closure_undefined_count = 8%nat.
Proof. vm_compute. reflexivity. Qed.

(** The closure component/source catalog has 15 entries (5 x 3). *)
Lemma closure_catalog_count_consistent : total_entry_count = 15%nat.
Proof. exact total_entry_count_value. Qed.

(** ================================================================
    Part C:  Supply + Eligibility recorded as PS_Partial (014CC integrated)
    ================================================================ *)

(** Supply's closure projection status is PS_Partial (was PS_Undefined). *)
Lemma supply_recorded_partial :
  closure_projection_status_of BC_Supply = PS_Partial.
Proof. reflexivity. Qed.

(** Eligibility/σ closure projection status is PS_Partial. *)
Lemma eligibility_recorded_partial :
  eligibility_closure_status = PS_Partial.
Proof. reflexivity. Qed.

(** The underlying 014CC projection statuses agree with the closure record. *)
Lemma supply_eligibility_status_match_014CC :
  supply_projection_status = closure_projection_status_of BC_Supply /\
  eligibility_projection_status = eligibility_closure_status.
Proof. split; reflexivity. Qed.

(** Neither Supply nor Eligibility is PS_Defined (no false equivalence). *)
Lemma supply_eligibility_not_defined :
  closure_projection_status_of BC_Supply <> PS_Defined /\
  eligibility_closure_status <> PS_Defined.
Proof. split; discriminate. Qed.

(** ================================================================
    Part D:  No false equivalence claim
    ================================================================ *)

(** Equivalence is NOT witnessed: there is no all-agreement (agreement < total),
    so the closure does not assert Coq = Runtime / Coq = Frozen C++. *)
Lemma closure_not_equivalence :
  e2e_total_agreement < length closure_all_results.
Proof. vm_compute. repeat constructor. Qed.

(** Concretely, the disagreement + missing verdicts are non-zero. *)
Lemma closure_residual_nonzero :
  e2e_total_disagreement <> 0%nat /\ e2e_total_missing <> 0%nat.
Proof. split; vm_compute; discriminate. Qed.

(** The Supply pair, although now carrying a present Coq observation, is still
    recorded as Missing on both sources (frozen + runtime absent) — an honest
    Missing, not a fabricated Agreement. *)
Lemma supply_pair_still_missing :
  compare_semantic_vs_frozen_observation sem_supply
    (snd supply_frozen_pair) = FAR_Missing /\
  compare_semantic_vs_live_runtime_observation sem_supply
    (snd supply_runtime_pair) = FAR_Missing.
Proof. split; reflexivity. Qed.

(** ================================================================
    Part E:  Remaining proof debt and circular dependencies
    ================================================================ *)

(** Proof-debt ledger for the closure audit. Each obligation is encoded as a
    boolean "closed?" flag; the closure proves they are all still OPEN/PARTIAL
    (false = not closed). *)
Record ProofDebtStatus := {
  pd_ob_02_closed : bool;   (* Coq = frozen C++ behaviour *)
  pd_cpp_01_closed : bool;  (* frozen C++ vs Coq equivalence *)
  pd_ob_03_closed : bool;   (* eligibility/σ re-derived (not imported) *)
}.

Definition closure_proof_debt : ProofDebtStatus :=
  {| pd_ob_02_closed := false;
     pd_cpp_01_closed := false;
     pd_ob_03_closed := false |}.

(** REQUIRED: the three equivalence/derivation debts remain NOT closed. *)
Lemma proof_debt_remains_open :
  closure_proof_debt.(pd_ob_02_closed) = false /\
  closure_proof_debt.(pd_cpp_01_closed) = false /\
  closure_proof_debt.(pd_ob_03_closed) = false.
Proof. repeat split; reflexivity. Qed.

(** All residual circular dependencies are explicitly recorded in the catalog:
    every component's recorded circular kind equals its [circular_of] value and
    is drawn from the explicit taxonomy. *)
Lemma circular_dependencies_recorded :
  forall e, In e closure_catalog ->
    e.(cce_circular) = circular_of e.(cce_component).
Proof. exact all_entries_circular_recorded. Qed.

(** The four resolved-by-obligation components (Draw/Replay/Reorg/ConnectBlock)
    carry a payload-domain or imported-eligibility residual; Supply carries the
    supply-absent residual. None is CDK_None among the five consensus components. *)
Lemma consensus_components_have_residual :
  circular_of BC_Draw = CDK_ImportedEligibility /\
  circular_of BC_Replay = CDK_PayloadDomainGap /\
  circular_of BC_Reorg = CDK_PayloadDomainGap /\
  circular_of BC_ConnectBlock = CDK_PayloadDomainGap /\
  circular_of BC_Supply = CDK_SupplyAbsent.
Proof. repeat split; reflexivity. Qed.

(** ================================================================
    Part F:  Closure summary
    ================================================================ *)

(** Master closure lemma: headline counts hold, projection counts are
    consistent, Supply/Eligibility are PS_Partial, and no equivalence/derivation
    debt is closed. *)
Lemma end_to_end_closure_summary :
  e2e_total_agreement = 0%nat /\
  e2e_total_disagreement = 8%nat /\
  e2e_total_missing = 2%nat /\
  closure_defined_count = 2%nat /\
  closure_partial_count = 6%nat /\
  closure_undefined_count = 0%nat /\
  closure_projection_status_of BC_Supply = PS_Partial /\
  eligibility_closure_status = PS_Partial /\
  closure_proof_debt.(pd_ob_02_closed) = false /\
  closure_proof_debt.(pd_cpp_01_closed) = false /\
  closure_proof_debt.(pd_ob_03_closed) = false.
Proof.
  split. exact e2e_total_agreement_value.
  split. exact e2e_total_disagreement_value.
  split. exact e2e_total_missing_value.
  split. exact closure_defined_count_value.
  split. exact closure_partial_count_value.
  split. exact closure_undefined_count_value.
  split. reflexivity.
  split. reflexivity.
  split. reflexivity.
  split. reflexivity.
  reflexivity.
Qed.
