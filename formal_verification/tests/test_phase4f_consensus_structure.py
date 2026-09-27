#!/usr/bin/env python3
"""
Structural validation for PHASE-4F end-to-end consensus formalization.

/001B/001C — checks Coq
module presence, named definitions/theorems, and phase4f doc sections.
Does not run coqc.
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
COQ_DIR = REPO_ROOT / "formal_verification" / "archive" / "lottery" / "coq" / "Phase4FConsensus"
COQ_DIR_4G = REPO_ROOT / "formal_verification" / "archive" / "lottery" / "coq" / "Phase4GRefinement"
COQ_DIR_5E = REPO_ROOT / "formal_verification" / "archive" / "lottery" / "coq" / "Phase5Equivalence"
DOC_DIR = REPO_ROOT / "docs" / "formal" / "phase4f_consensus"
DOC_DIR_5E = REPO_ROOT / "docs" / "formal" / "phase5_equivalence"
COQ_PROJECT = REPO_ROOT / "formal_verification" / "coq" / "_CoqProject"

FORBIDDEN_IMPORTS = (
    "RigorSha256",
    "ConvergeRoot",
    "RigorHash",
    "SigmaCR01",
    "Execution",
    "ConnectBlock",
)

MODULE_EXPECTATIONS: dict[str, list[str]] = {
    "ConsensusMachine.v": [
        "Record ConsensusState",
        "Inductive ConsensusTransition",
        "Definition execute_transition",
        "Fixpoint execute_trace",
    ],
    "ConsensusTrace.v": [
        "Inductive ValidTraceStep",
        "Definition valid_trace",
        "Lemma empty_trace_identity",
        "Theorem execute_trace_append",
    ],
    "ConsensusDeterminism.v": [
        "Theorem execute_transition_deterministic",
        "Theorem execute_trace_deterministic",
        "Theorem consensus_replay_determinism",
        "Theorem oracle_agreement_placeholder",
    ],
    "ConsensusGoldenTrace.v": [
        "Record TraceCase",
        "Record TraceInput",
        "Record TraceExpected",
        "Record GoldenTraceResult",
        "Record ReplayResult",
        "Record GoldenVectorMeta",
        "Inductive TraceExecutionStatus",
        "Definition mk_golden_trace_case",
        "Definition meta_golden_match",
        "Lemma golden_trace_result_match_is_meta",
    ],
    "ConsensusDrawProjection.v": [
        "Inductive DrawProjectionStatus",
        "Definition project_draw_result",
        "Definition compare_projection_to_golden",
    ],
    "ConsensusGoldenVectorsData.v": [
        "Definition generated_trace_case_count",
        "Definition trace_cases_PHASE3_CR",
        "Definition all_registry_trace_cases",
        "Lemma all_registry_trace_cases_length",
    ],
    "ConsensusVectorRegistry.v": [
        "Inductive VectorFamily",
        "Record VectorFamilyEntry",
        "Definition vector_registry",
        "Definition family_vector_count",
        "FCS_PLACEHOLDER",
    ],
    "ConsensusTraceExecution.v": [
        "Definition execute_trace_case",
        "Definition execute_vector_family",
        "Definition execute_registry",
        "Definition trace_cases_for_family",
        "Lemma registry_execution_count_eq_total",
        "Definition registry_match_count",
    ],
    "ConsensusDrawAudit.v": [
        "Definition IndependentWinnerSource",
        "Definition ImportedWinnerSource",
        "Inductive WinnerComparisonResult",
        "Definition winner_sources_agree",
        "Definition winner_sources_disagree",
        "Definition recompute_winner",
    ],
    "ConsensusOracleBridge.v": [
        "Inductive OracleResult",
        "Record OracleObservation",
        "Record TraceObservation",
        "Inductive ObservationAgreement",
        "Inductive BridgeStatus",
        "Definition compare_observation",
    ],
    "ConsensusRuntimeEvidence.v": [
        "Inductive EvidenceSource",
        "Inductive ResEvidenceTask",
        "Definition phase_evidence_catalog",
        "Definition res_evidence_catalog",
    ],
    "RuntimeEvidenceBridge.v": [
        "Inductive RuntimeEvidenceKind",
        "Record RuntimeEvidenceRecord",
        "Definition evidence_to_observation",
        "REK_DrawWinner",
        "REK_ResMintAccepted",
        "REK_ConnectBlockAccepted",
        "REK_ReorgReplay",
        "REK_MultinodeConsensus",
    ],
    "RuntimeEvidenceCatalog.v": [
        "Definition runtime_evidence_catalog",
        "Definition runtime_evidence_count",
        "Lemma runtime_catalog_nonempty",
    ],
    "RuntimeObservationAudit.v": [
        "Definition runtime_observation_status",
        "ROS_Present",
        "ROS_Missing",
        "Lemma present_observation_not_missing",
        "Lemma catalog_entries_produce_present_observations",
    ],
    "TraceObservationBridge.v": [
        "Inductive TraceObservationKind",
        "Record TraceObservationRecord",
        "Definition trace_to_observation",
        "OS_CoqTrace",
    ],
    "TraceObservationCatalog.v": [
        "Definition trace_observation_catalog",
        "Definition trace_observation_count",
        "Lemma trace_catalog_nonempty",
    ],
    "TraceAgreementAudit.v": [
        "Definition agreement_result",
        "Definition agreement_status",
        "Definition agreement_count",
        "Definition disagreement_count",
        "Definition missing_count",
        "compare_observation",
        "Theorem agree_implies_same_payload",
        "Theorem missing_not_agree",
        "Theorem disagree_not_match",
    ],
    "DrawRepresentationBridge.v": [
        "Definition winner_index_to_txid",
        "Definition winner_txid_projection",
        "Inductive DrawRepresentationAgreement",
    ],
    "DrawRepresentationCatalog.v": [
        "Definition representation_catalog",
        "Lemma representation_catalog_nonempty",
    ],
    "DrawRepresentationAudit.v": [
        "Definition representation_match_count",
        "Definition representation_mismatch_count",
        "Lemma representation_match_count_value",
        "Lemma representation_mismatch_count_value",
        "Theorem winner_projection_sound",
    ],
    "FrozenCppOracle.v": [
        "Inductive FrozenCppComponent",
        "Record FrozenCppObservation",
        "Inductive FrozenCppStatus",
        "FCS_Present",
        "FCS_Missing",
    ],
    "FrozenCppCatalog.v": [
        "Definition frozen_cpp_catalog",
    ],
    "FrozenCppAudit.v": [
        "Definition frozen_cpp_count",
        "Definition frozen_cpp_present_count",
        "Lemma catalog_nonempty",
        "Lemma catalog_present_entries_exist",
    ],
    "FrozenCppAgreement.v": [
        "Inductive FrozenAgreementResult",
        "Definition compare_frozen_observation",
        "Definition agreement_count",
        "Definition disagreement_count",
        "Definition missing_count",
        "FAR_Agree",
        "FAR_Disagree",
        "FAR_Missing",
    ],
    "FrozenCppAgreementCatalog.v": [
        "Definition frozen_agreement_pairs",
    ],
    "FrozenCppAgreementAudit.v": [
        "Lemma catalog_nonempty",
        "Lemma agreement_statistics_exist",
        "Lemma frozen_agreement_count_value",
    ],
    "FrozenCppTraceComparison.v": [
        "FrozenTraceAgreementResult",
        "Definition compare_frozen_vs_coq_observation",
        "trace_agreement_count",
        "trace_disagreement_count",
        "trace_missing_count",
    ],
    "FrozenCppTraceCatalog.v": [
        "Definition frozen_trace_pairs",
        "winner_index",
    ],
    "FrozenCppTraceAudit.v": [
        "Lemma catalog_nonempty",
        "Lemma agreement_statistics_exist",
        "Lemma frozen_trace_agreement_count_value",
        "live_cross_agreement_count_value",
    ],
    "LiveRuntimeTraceIntegration.v": [
        "Record LiveTraceObservation",
        "Definition LiveTraceObservationCatalog",
        "Definition project_live_runtime_to_coq_trace",
        "Definition compare_live_vs_frozen_observation",
        "Definition live_agreement_count",
        "Definition live_disagreement_count",
        "Definition live_missing_count",
        "Lemma compare_agree_of_equal",
        "Lemma live_agreement_count_value",
    ],
    "SemanticObservationDerivation.v": [
        "Record SemanticObservation",
        "Definition derive_draw_observation",
        "Definition derive_replay_observation",
        "Definition derive_reorg_observation",
        "Definition derive_connectblock_observation",
    ],
    "SemanticObservationCatalog.v": [
        "Definition semantic_observation_catalog",
    ],
    "SemanticObservationAudit.v": [
        "Definition semantic_observation_count",
        "Definition draw_semantic_count",
        "Definition replay_semantic_count",
        "Definition reorg_semantic_count",
        "Definition connectblock_semantic_count",
        "Lemma catalog_nonempty",
        "Lemma semantic_entries_exist",
    ],
    "SemanticVsFrozenComparison.v": [
        "Definition compare_semantic_vs_frozen_observation",
        "Definition semantic_trace_agreement_count",
        "Definition semantic_trace_disagreement_count",
        "Definition semantic_trace_missing_count",
        "Lemma compare_semantic_vs_frozen_observation_sound",
    ],
    "SemanticVsFrozenCatalog.v": [
        "Definition semantic_vs_frozen_pairs",
    ],
    "SemanticVsFrozenAudit.v": [
        "Definition agreement_count",
        "Definition disagreement_count",
        "Definition missing_count",
        "Lemma catalog_nonempty",
        "Lemma semantic_vs_frozen_entries_exist",
    ],
    "SemanticVsLiveRuntimeComparison.v": [
        "Definition compare_semantic_vs_live_runtime_observation",
        "Definition semantic_trace_vs_runtime_agreement_count",
        "Definition semantic_trace_vs_runtime_disagreement_count",
        "Definition semantic_trace_vs_runtime_missing_count",
        "Lemma compare_semantic_vs_live_runtime_observation_sound",
    ],
    "SemanticVsLiveRuntimeCatalog.v": [
        "Definition semantic_vs_runtime_pairs",
    ],
    "SemanticVsLiveRuntimeAudit.v": [
        "Definition agreement_count",
        "Definition disagreement_count",
        "Definition missing_count",
        "Lemma catalog_nonempty",
        "Lemma semantic_vs_runtime_entries_exist",
    ],
    "Phase4FEndToEndComparison.v": [
        "Record E2EComponentSummary",
        "Definition per_component_summary",
        "Definition compute_final_audit_counts",
        "Lemma unified_counts_decompose",
    ],
    "Phase4FEndToEndCatalog.v": [
        "Record E2ECatalogEntry",
        "Definition e2e_catalog",
    ],
    "Phase4FEndToEndAudit.v": [
        "Definition total_agreement",
        "Definition total_disagreement",
        "Definition total_missing",
        "Lemma catalog_nonempty",
        "Lemma audit_entries_exist",
    ],
}

MODULE_EXPECTATIONS_4G: dict[str, list[str]] = {
    "SupplyEligibilityProjection.v": [
        "Definition project_supply_payload",
        "Definition project_supply_after_trace",
        "Definition project_eligibility_sigma",
        "Definition project_eligibility_count",
        "Lemma init_supply_zero",
        "Lemma transition_preserves_supply",
        "Lemma execute_trace_preserves_supply",
        "Lemma project_supply_after_trace_zero",
        "Theorem supply_projection_structural_identity",
        "Lemma project_eligibility_sigma_length",
        "Lemma project_eligibility_sigma_nth",
        "Theorem eligibility_projection_is_identity_map",
        "Definition supply_projection_status",
        "Definition eligibility_projection_status",
    ],
    "SupplyEligibilityCatalog.v": [
        "Definition derive_supply_observation",
        "Definition sem_supply",
        "Definition supply_frozen_pair",
        "Definition supply_runtime_pair",
        "Definition supply_eligibility_semantic_vs_frozen_pairs",
        "Definition supply_eligibility_semantic_vs_runtime_pairs",
        "Record EligibilityCatalogEntry",
        "Definition build_eligibility_entry",
        "Definition eligibility_catalog",
        "Lemma eligibility_catalog_identity",
        "Lemma eligibility_catalog_length",
    ],
    "SupplyEligibilityAudit.v": [
        "Definition supply_vs_frozen_result",
        "Definition supply_vs_runtime_result",
        "Definition full_sf_results",
        "Definition full_sr_results",
        "Definition full_sf_agreement_count",
        "Definition full_sf_disagreement_count",
        "Definition full_sf_missing_count",
        "Definition full_sr_agreement_count",
        "Definition full_sr_disagreement_count",
        "Definition full_sr_missing_count",
        "Lemma full_sf_disagreement_count_value",
        "Lemma full_sr_disagreement_count_value",
        "Lemma e2e_total_disagreement_value",
        "Lemma e2e_total_missing_value",
        "Lemma eligibility_audit_total_value",
        "Lemma all_eligibility_rows_identity_value",
        "Lemma audit_counts_match_phase4f_e2e",
        "Lemma supply_and_eligibility_partial_status_after_014CC",
    ],
}

# End-to-End Closure consolidation modules.
MODULE_EXPECTATIONS_4G_CLOSURE: dict[str, list[str]] = {
    "EndToEndClosureCatalog.v": [
        "Inductive ClosureSource",
        "Inductive CircularDependencyKind",
        "Definition closure_projection_status_of",
        "Definition circular_of",
        "Record ClosureCatalogEntry",
        "Definition closure_components",
        "Definition closure_catalog",
        "Definition total_entry_count",
        "Definition eligibility_closure_status",
        "Lemma closure_status_supply_upgraded",
        "Lemma eligibility_closure_status_partial",
        "Lemma catalog_nonempty",
        "Lemma total_entry_count_value",
        "Lemma all_entries_mapped",
        "Lemma all_entries_projection_consistent",
        "Lemma all_entries_circular_recorded",
    ],
    "EndToEndClosureAudit.v": [
        "Definition closure_sf_results",
        "Definition closure_sr_results",
        "Definition closure_all_results",
        "Definition e2e_total_agreement",
        "Definition e2e_total_disagreement",
        "Definition e2e_total_missing",
        "Lemma e2e_total_agreement_value",
        "Lemma e2e_total_disagreement_value",
        "Lemma e2e_total_missing_value",
        "Lemma e2e_counts_partition",
        "Definition closure_defined_count",
        "Definition closure_partial_count",
        "Definition closure_undefined_count",
        "Lemma closure_defined_count_value",
        "Lemma closure_partial_count_value",
        "Lemma closure_undefined_count_value",
        "Lemma closure_projection_counts_consistent",
        "Lemma closure_catalog_count_consistent",
        "Lemma supply_recorded_partial",
        "Lemma eligibility_recorded_partial",
        "Lemma supply_eligibility_not_defined",
        "Lemma closure_not_equivalence",
        "Lemma supply_pair_still_missing",
        "Record ProofDebtStatus",
        "Definition closure_proof_debt",
        "Lemma proof_debt_remains_open",
        "Lemma circular_dependencies_recorded",
        "Lemma consensus_components_have_residual",
        "Lemma end_to_end_closure_summary",
    ],
}

# Final Readiness Verdict (frozen re-export of 015DD).
MODULE_EXPECTATIONS_4G_FINAL: dict[str, list[str]] = {
    "FinalReadinessCatalog.v": [
        "Inductive FinalProvenance",
        "Definition provenance_of",
        "Record FinalEntry",
        "Definition mk_final_entry",
        "Definition final_catalog",
        "Definition total_entry_count",
        "Lemma catalog_nonempty",
        "Lemma total_entry_count_value",
        "Lemma all_entries_mapped",
        "Lemma all_final_entries_projection_consistent",
        "Lemma all_final_entries_circular_recorded",
        "Lemma final_provenance_matches_source",
    ],
    "FinalReadinessVerdict.v": [
        "Definition final_total_agreement",
        "Definition final_total_disagreement",
        "Definition final_total_missing",
        "Definition final_verdict_total",
        "Lemma final_total_agreement_value",
        "Lemma final_total_disagreement_value",
        "Lemma final_total_missing_value",
        "Lemma final_verdict_total_value",
        "Lemma final_counts_partition",
        "Definition final_defined_count",
        "Definition final_partial_count",
        "Definition final_undefined_count",
        "Lemma final_defined_count_value",
        "Lemma final_partial_count_value",
        "Lemma final_undefined_count_value",
        "Lemma final_projection_total_eight",
        "Definition final_status",
        "Lemma final_status_draw",
        "Lemma final_status_replay",
        "Lemma final_status_reorg",
        "Lemma final_status_connectblock",
        "Lemma final_status_supply",
        "Definition final_eligibility_status",
        "Lemma final_eligibility_status_value",
        "Lemma final_supply_eligibility_partial",
        "Definition final_circular",
        "Lemma final_circular_consensus_components_recorded",
        "Record FinalProofDebt",
        "Definition final_proof_debt",
        "Lemma final_proof_debt_remains_open",
        "Lemma final_no_equivalence",
        "Inductive ReadinessVerdict",
        "Definition final_verdict",
        "Lemma final_verdict_is_partial",
        "Lemma final_verdict_not_equivalent",
        "Lemma final_readiness_verdict",
    ],
    "FinalReadinessAudit.v": [
        "Record FinalComponentRow",
        "Definition mk_component_row",
        "Definition final_component_table",
        "Lemma final_component_table_length",
        "Lemma final_component_table_components",
        "Lemma final_component_table_statuses",
        "Lemma final_component_table_circular",
        "Definition source_count",
        "Definition frozen_count",
        "Definition runtime_count",
        "Definition semantic_count",
        "Lemma source_count_balanced",
        "Lemma source_counts_sum_to_total",
        "Record FinalVerdictRow",
        "Definition final_verdict_table",
        "Lemma final_verdict_table_counts",
        "Record FinalProjectionRow",
        "Definition final_projection_table",
        "Lemma final_projection_table_counts",
        "Inductive ProofDebtId",
        "Record FinalProofDebtRow",
        "Definition final_proof_debt_table",
        "Lemma final_proof_debt_all_open",
        "Lemma final_audit_no_equivalence",
        "Lemma final_audit_summary",
        "Lemma final_audit_circular_table",
        "Lemma final_audit_verdict_partial",
        "Lemma final_audit_verdict_not_equivalent",
    ],
}

# Formal Verification Release Certification module.
MODULE_EXPECTATIONS_4G_CERT: dict[str, list[str]] = {
    "FormalVerificationCertification.v": [
        "Inductive CertificationStatus",
        "CS_ArtifactsConsolidated",
        "CS_NotReady",
        "Definition cert_status_eqb",
        "Definition verdict_constructor_ok",
        "Definition verdict_counts_ok",
        "Definition projection_counts_ok",
        "Definition catalog_total_ok",
        "Definition supply_eligibility_ok",
        "Definition proof_debt_open_ok",
        "Definition release_predicate",
        "Definition certification_status",
        "Lemma verdict_constructor_ok_value",
        "Lemma verdict_counts_ok_value",
        "Lemma projection_counts_ok_value",
        "Lemma catalog_total_ok_value",
        "Lemma supply_eligibility_ok_value",
        "Lemma proof_debt_open_ok_value",
        "Lemma release_predicate_holds",
        "Theorem formal_verification_ready",
        "Lemma certification_does_not_claim_equivalence",
        "Lemma certification_records_open_proof_debt",
        "Lemma certification_records_no_equivalence",
        "Lemma certification_partial_only",
    ],
}

# Phase-5 equivalence architecture scaffold.
MODULE_EXPECTATIONS_5E: dict[str, list[str]] = {
    "EquivalenceFramework.v": [
        "Inductive EquivalenceSource",
        "Inductive EquivalenceClaimStatus",
        "ECS_Pending",
        "Inductive ComponentEquivalenceObligation",
        "Definition component_obligation_table",
        "Definition frozen_cpp_oracle_wired",
        "Definition equivalence_claimed",
        "Lemma equivalence_not_claimed",
        "Lemma proof_debt_all_open",
        "Lemma framework_established_holds",
    ],
    "EquivalenceCatalog.v": [
        "Record EquivalenceCatalogEntry",
        "Definition equivalence_catalog",
        "Lemma catalog_entry_count_value",
        "Lemma catalog_resolved_count_value",
        "Lemma all_catalog_entries_pending",
    ],
    "EquivalenceAudit.v": [
        "Definition resolved_component_obligations",
        "Lemma resolved_component_obligations_value",
        "Lemma resolved_catalog_cells_value",
        "Definition equivalence_framework_ready",
        "Lemma equivalence_framework_ready_holds",
        "Lemma audit_no_equivalence_theorem",
    ],
}

MODULE_EXPECTATIONS_5E_GG_N: dict[str, list[str]] = {
    "CanonicalRuntimeHashAlignmentCatalog.v": [
        "Inductive CanonicalHashAlignmentStatus",
        "Definition ias_to_chas",
        "Definition component_canonical_hash",
        "Record CanonicalRuntimeHashAlignmentEntry",
        "Definition canonical_runtime_hash_alignment_catalog",
        "Record ComponentCanonicalHashAlignmentSummary",
        "Lemma chas_reflects_prior_ias",
        "Lemma canonical_coq_hash_not_runtime_block_hash",
        "Lemma canonical_runtime_hash_alignment_catalog_nonempty",
        "Lemma canonical_runtime_hash_alignment_partition_complete",
        "Lemma canonical_runtime_hash_alignment_status_defined",
        "Lemma canonical_runtime_hash_alignment_preserves_open_proof_debt",
        "Lemma canonical_runtime_hash_alignment_does_not_establish_equivalence",
        "Lemma draw_canonical_hash_alignment_fundamentally_blocked",
    ],
    "CanonicalRuntimeHashAlignmentAudit.v": [
        "Definition crhaa_total_fields",
        "Lemma crhaa_total_fields_value",
        "Lemma crhaa_partition_complete",
        "Lemma audit_canonical_runtime_hash_alignment_partition_complete",
        "Lemma audit_canonical_runtime_hash_alignment_does_not_establish_equivalence",
        "Lemma audit_canonical_runtime_hash_alignment_preserves_open_proof_debt",
        "Lemma canonical_runtime_hash_alignment_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_M: dict[str, list[str]] = {
    "RuntimeAlignmentIntegrationCatalog.v": [
        "Inductive IntegratedAlignmentStatus",
        "Definition combine_cas_ifs",
        "Record RuntimeAlignmentIntegrationEntry",
        "Definition runtime_alignment_integration_catalog",
        "Record ComponentAlignmentIntegrationSummary",
        "Lemma alignment_integration_preserves_cas_partial",
        "Lemma combine_preserves_ifs_on_partial_components",
        "Lemma runtime_alignment_integration_catalog_nonempty",
        "Lemma runtime_alignment_integration_partition_complete",
        "Lemma runtime_alignment_integration_status_defined",
        "Lemma runtime_alignment_integration_preserves_open_proof_debt",
        "Lemma runtime_alignment_integration_does_not_establish_equivalence",
        "Lemma draw_alignment_integration_fundamentally_blocked",
    ],
    "RuntimeAlignmentIntegrationAudit.v": [
        "Definition raia_total_fields",
        "Lemma raia_total_fields_value",
        "Lemma raia_partition_complete",
        "Lemma audit_runtime_alignment_integration_partition_complete",
        "Lemma audit_runtime_alignment_integration_does_not_establish_equivalence",
        "Lemma audit_runtime_alignment_integration_preserves_open_proof_debt",
        "Lemma runtime_alignment_integration_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_L: dict[str, list[str]] = {
    "RuntimeWitnessIntegrationCatalog.v": [
        "Inductive FieldAvailabilityAnnotation",
        "Inductive IntegratedFieldStatus",
        "Definition witness_maps_to_canonical_hash",
        "Definition integrated_coq_hash",
        "Record RuntimeWitnessIntegrationEntry",
        "Definition runtime_witness_integration_catalog",
        "Record ComponentIntegrationSummary",
        "Lemma integration_preserves_cas_partial_alignment",
        "Lemma runtime_witness_integration_catalog_nonempty",
        "Lemma runtime_witness_integration_partition_complete",
        "Lemma runtime_witness_integration_status_defined",
        "Lemma runtime_witness_integration_preserves_open_proof_debt",
        "Lemma runtime_witness_integration_does_not_establish_equivalence",
        "Lemma draw_integration_fundamentally_blocked",
    ],
    "RuntimeWitnessIntegrationAudit.v": [
        "Definition rwia_total_fields",
        "Lemma rwia_total_fields_value",
        "Lemma rwia_partition_complete",
        "Lemma audit_runtime_witness_integration_partition_complete",
        "Lemma audit_runtime_witness_integration_does_not_establish_equivalence",
        "Lemma audit_runtime_witness_integration_preserves_open_proof_debt",
        "Lemma runtime_witness_integration_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_K: dict[str, list[str]] = {
    "RuntimeAlignmentCatalog.v": [
        "Inductive FieldAlignmentStatus",
        "Inductive ComponentAlignmentStatus",
        "Record RuntimeAlignmentEntry",
        "Definition classify_field_alignment",
        "Definition runtime_alignment_catalog",
        "Record HashProjectionAlignment",
        "Definition hash_projection_alignments",
        "Lemma hash_projection_not_runtime_equal",
        "Lemma runtime_alignment_catalog_nonempty",
        "Lemma runtime_alignment_partition_complete",
        "Lemma runtime_alignment_status_defined",
        "Lemma runtime_alignment_preserves_open_proof_debt",
        "Lemma runtime_alignment_does_not_establish_equivalence",
        "Lemma draw_fundamentally_blocked",
        "Lemma replay_reorg_cb_partial_alignment",
    ],
    "RuntimeAlignmentAudit.v": [
        "Definition raa_total_fields",
        "Lemma raa_total_fields_value",
        "Lemma raa_partition_complete",
        "Lemma audit_runtime_alignment_partition_complete",
        "Lemma audit_runtime_alignment_does_not_establish_equivalence",
        "Lemma audit_runtime_alignment_preserves_open_proof_debt",
        "Lemma audit_hash_projection_not_equated",
        "Lemma runtime_alignment_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_FVR_N: dict[str, list[str]] = {
    "Phase5SummaryCatalog.v": [
        "Inductive Phase5LayerStatus",
        "Record Phase5LayerEntry",
        "Definition phase5_layer_catalog",
        "Definition phase5_freeze_protocol_link",
        "Lemma phase5_layer_partition_complete",
        "Lemma phase5_runtime_layers_established",
        "Lemma draw_permanently_non_closable",
        "Lemma all_other_layers_active",
        "Lemma coq_equiv_runtime_not_established",
        "Lemma equivalence_claimed_false",
        "Lemma phase5_summary_does_not_establish_equivalence",
        "Lemma phase5_summary_preserves_open_proof_debt",
        "Lemma phase5_summary_links_freeze_protocol",
    ],
    "Phase5SummaryAudit.v": [
        "Definition p5s_layer_count",
        "Lemma p5s_partition_complete",
        "Lemma audit_phase5_runtime_layers_established",
        "Lemma audit_draw_permanently_non_closable",
        "Lemma audit_coq_equiv_runtime_not_established",
        "Lemma audit_equivalence_claimed_false",
        "Lemma audit_phase5_summary_preserves_open_proof_debt",
        "Lemma phase5_summary_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_019: dict[str, list[str]] = {
    "ProofObligationDependencyCatalog.v": [
        "Inductive DependencyStatus",
        "Inductive DependencyObject",
        "A1_SigmaByteOrder",
        "A2_Uint512Bound",
        "A3_FMembershipPredicate",
        "A4_SHA256dEquivalence",
        "A5_StatusAuthority",
        "PD15_SigmaIntegerRefinement",
        "PD14_Uint512MagnitudeBound",
        "PD13_BuildFEnumerationCorrectness",
        "PD4F_OB_02",
        "PD4F_CPP_01",
        "Definition proof_obligation_02_closable",
        "Definition pd4f_ob02_ready",
        "Definition pd4f_ob02_permitted_status",
        "Theorem pd4f_ob02_dependency_soundness",
        "Theorem pd4f_ob02_not_closed_without_cpp01",
        "Lemma proof_obligation_02_closable_at_baseline",
        "Lemma pd4f_ob02_does_not_establish_equivalence",
    ],
    "ProofObligationDependencyAudit.v": [
        "Definition poda_object_count",
        "Definition poda_edge_count",
        "Lemma poda_object_count_value",
        "Lemma poda_edge_count_value",
        "Lemma audit_proof_obligation_02_closable_at_baseline",
        "Lemma audit_pd4f_ob02_dependency_soundness",
        "Lemma audit_pd4f_ob02_not_closed_without_cpp01",
        "Lemma audit_pd4f_ob02_does_not_establish_equivalence",
        "Definition proof_obligation_dependency_audit_verdict",
        "Lemma proof_obligation_dependency_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_020: dict[str, list[str]] = {
    "SigmaIntegerRefinement.v": [
        "Fixpoint sigma_le_sum_nat",
        "Definition sigma_bytes_to_integer",
        "Lemma sigma_le_sum_nat_eq_go",
        "Lemma sigma_bytes_to_integer_deterministic",
        "Lemma sigma_bytes_to_integer_injective_length_fixed",
        "Lemma runtime_spec_consistency",
        "Definition pd15_closable",
        "Theorem pd15_refinement_soundness",
        "Lemma pd15_dependency_status_closed",
        "Lemma pd14_dependency_status_closed",
        "Lemma pd13_dependency_status_closed",
        "Lemma pd4f_ob02_dependency_status_ready",
    ],
    "SigmaIntegerRefinementAudit.v": [
        "Definition sia_sigma_byte_length",
        "Lemma audit_sigma_bytes_to_integer_deterministic",
        "Lemma audit_runtime_spec_consistency",
        "Lemma audit_pd15_refinement_soundness",
        "Lemma audit_pd15_dependency_status_closed",
        "Definition sigma_integer_refinement_audit_verdict",
        "Lemma sigma_integer_refinement_audit_verdict_holds",
    ],
    "ProofObligationDependencyCatalog.v": [
        "Lemma baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
    ],
    "ProofObligationDependencyAudit.v": [
        "Lemma audit_baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_021: dict[str, list[str]] = {
    "UInt512OverflowRefinement.v": [
        "Definition uint512_value_bound",
        "Definition mul256x256_full",
        "Definition fvr_mul256x256_full",
        "Definition uint512_bytes_to_integer",
        "Lemma mul256x256_full_deterministic",
        "Lemma uint512_bytes_to_integer_injective_length_fixed",
        "Lemma runtime_spec_consistency",
        "Lemma convergence_product_bounded",
        "Definition pd14_closable",
        "Theorem pd14_refinement_soundness",
        "Lemma pd14_dependency_status_closed",
        "Lemma pd15_baseline_closed_from_catalog",
        "Lemma pd13_dependency_status_closed",
        "Lemma pd4f_ob02_dependency_status_ready",
    ],
    "UInt512OverflowRefinementAudit.v": [
        "Definition uoa_uint512_byte_length",
        "Lemma audit_mul256x256_full_deterministic",
        "Lemma audit_runtime_spec_consistency",
        "Lemma audit_pd14_refinement_soundness",
        "Lemma audit_pd14_dependency_status_closed",
        "Definition uint512_overflow_refinement_audit_verdict",
        "Lemma uint512_overflow_refinement_audit_verdict_holds",
    ],
    "ProofObligationDependencyCatalog.v": [
        "Lemma baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
    ],
    "ProofObligationDependencyAudit.v": [
        "Lemma audit_baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_022: dict[str, list[str]] = {
    "FSetMembershipRefinement.v": [
        "Record FMembershipTx",
        "Definition tx_passes_f_predicate",
        "Definition coq_enumerate_F",
        "Definition F_membership",
        "Lemma coq_enumerate_F_deterministic",
        "Lemma coq_enumerate_F_nodup",
        "Lemma runtime_spec_consistency",
        "Lemma F_membership_sound",
        "Definition pd13_closable",
        "Theorem pd13_refinement_soundness",
        "Lemma pd13_dependency_status_closed",
        "Lemma pd14_baseline_closed_from_catalog",
        "Lemma pd15_baseline_closed_from_catalog",
        "Lemma pd4f_ob02_dependency_status_ready",
        "Lemma pd4f_cpp01_dependency_status_open",
    ],
    "FSetMembershipRefinementAudit.v": [
        "Definition fma_min_convergence_amount",
        "Lemma audit_coq_enumerate_F_deterministic",
        "Lemma audit_runtime_spec_consistency",
        "Lemma audit_F_membership_sound",
        "Lemma audit_pd13_refinement_soundness",
        "Lemma audit_pd13_dependency_status_closed",
        "Definition fset_membership_refinement_audit_verdict",
        "Lemma fset_membership_refinement_audit_verdict_holds",
    ],
    "ProofObligationDependencyCatalog.v": [
        "Lemma baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
        "Lemma proof_obligation_02_closable_at_baseline",
        "Lemma pd4f_ob02_ready_at_baseline",
    ],
    "ProofObligationDependencyAudit.v": [
        "Lemma audit_baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
        "Lemma audit_proof_obligation_02_closable_at_baseline",
        "Lemma audit_pd4f_ob02_ready_at_baseline",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_023: dict[str, list[str]] = {
    "PD4FOB02Readiness.v": [
        "Theorem pd4f_ob02_ready_soundness",
        "Lemma pd4f_ob02_ready_not_closed",
        "Theorem pd4f_ob02_not_closed_without_cpp01_holds",
        "Lemma pd4f_ob02_ready_soundness_frozen",
        "Lemma pd4f_cpp01_unresolved_at_baseline",
        "Lemma pd13_baseline_closed_true",
        "Lemma pd4f_ob02_baseline_ready_true",
        "Lemma pd4f_cpp01_baseline_open_true",
    ],
    "PD4FOB02ReadinessAudit.v": [
        "Definition audit_pd4f_ob02_ready",
        "Lemma audit_pd4f_ob02_ready_soundness",
        "Lemma audit_pd4f_ob02_ready_not_closed",
        "Lemma audit_pd4f_ob02_not_closed_without_cpp01",
        "Lemma audit_pd4f_cpp01_unresolved",
        "Definition pd4f_ob02_readiness_audit_verdict",
        "Lemma pd4f_ob02_readiness_audit_verdict_holds",
    ],
    "ProofObligationDependencyCatalog.v": [
        "PD4F_CPP_01",
        "Lemma baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
        "Lemma pd4f_ob02_ready_at_baseline",
        "Theorem pd4f_ob02_not_closed_without_cpp01",
    ],
    "ProofObligationDependencyAudit.v": [
        "Lemma audit_baseline_pd13_pd14_pd15_closed_ob02_ready_cpp01_open",
        "Lemma audit_pd4f_ob02_ready_at_baseline",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_024: dict[str, list[str]] = {
    "PD4FCPP01ClosureModel.v": [
        "Definition cpp01_runtime_alignment_ready",
        "Definition cpp01_vector_alignment_ready",
        "Definition cpp01_coq_alignment_ready",
        "Definition cpp01_candidate_equivalence_ready",
        "Definition pd4f_cpp01_closable",
        "Theorem pd4f_cpp01_requires_ob02_ready",
        "Theorem pd4f_cpp01_not_closable_from_ob02_alone",
        "Theorem pd4f_cpp01_baseline_open",
        "Lemma pd4f_cpp01_closable_frozen_false",
    ],
    "PD4FCPP01ClosureAudit.v": [
        "Definition audit_cpp01_runtime_alignment_ready",
        "Lemma audit_cpp01_runtime_alignment_ready_baseline",
        "Lemma audit_pd4f_cpp01_requires_ob02_ready",
        "Lemma audit_pd4f_cpp01_not_closable_from_ob02_alone",
        "Lemma audit_pd4f_cpp01_baseline_open",
        "Definition pd4f_cpp01_closure_audit_verdict",
        "Lemma pd4f_cpp01_closure_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_025: dict[str, list[str]] = {
    "CandidateEquivalenceEvidenceModel.v": [
        "Inductive EvidenceCategory",
        "Inductive EvidenceState",
        "Definition candidate_equivalence_evidence_state",
        "Definition candidate_equivalence_evidence_ready",
        "Definition candidate_equivalence_ready",
        "Theorem candidate_equivalence_ready_requires_ob02",
        "Theorem candidate_equivalence_ready_forbids_cpp01_closed",
        "Definition candidate_equivalence_not_equivalence",
        "Definition candidate_equivalence_requires_cpp01",
        "Theorem candidate_equivalence_baseline_false",
    ],
    "CandidateEquivalenceEvidenceAudit.v": [
        "Definition audit_candidate_equivalence_evidence_ready",
        "Lemma audit_cee_runtime_alone_insufficient",
        "Lemma audit_cee_golden_vector_alone_insufficient",
        "Lemma audit_cee_coq_alone_insufficient",
        "Lemma audit_cpp01_candidate_gate_baseline_aligned",
        "Lemma audit_candidate_equivalence_not_equivalence",
        "Definition candidate_equivalence_evidence_audit_verdict",
        "Lemma candidate_equivalence_evidence_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_026: dict[str, list[str]] = {
    "CandidateEquivalenceRuntimeModel.v": [
        "Definition cee_runtime_fvr_mirror_layers",
        "Definition cee_runtime_golden_vector_catalog",
        "Definition candidate_equivalence_runtime_category_state",
        "Definition candidate_equivalence_runtime_evidence_ready",
        "Definition candidate_equivalence_runtime_ready",
        "Definition candidate_equivalence_runtime_not_equivalence",
        "Theorem candidate_equivalence_runtime_ready_requires_ob02",
        "Theorem candidate_equivalence_runtime_ready_forbids_cpp01_closed",
        "Theorem candidate_equivalence_runtime_baseline_false",
        "Definition candidate_equivalence_runtime_witness_schema",
    ],
    "CandidateEquivalenceRuntimeAudit.v": [
        "Definition audit_cpp01_runtime_alignment_gate",
        "Lemma audit_cpp01_runtime_alignment_verified",
        "Lemma audit_cpp01_vector_alignment_verified",
        "Lemma audit_cpp01_coq_alignment_verified",
        "Lemma audit_cpp01_candidate_equivalence_remains_false",
        "Lemma audit_candidate_equivalence_runtime_not_equivalence",
        "Definition candidate_equivalence_runtime_audit_verdict",
        "Lemma candidate_equivalence_runtime_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_027: dict[str, list[str]] = {
    "Phase5SummaryFinalCatalog.v": [
        "Definition p5_final_freeze_protocol_link",
        "Definition p5_final_fvr_layer_count",
        "Definition p5_final_ecs_open_count",
        "Definition p5_final_candidate_runtime_ready",
        "Theorem draw_permanently_non_closable",
        "Theorem phase5_final_summary_verdict_holds",
        "Definition p5_final_json_export_rpc",
    ],
    "Phase5SummaryFinalAudit.v": [
        "Definition audit_p5_final_candidate_runtime_ready",
        "Lemma audit_p5_final_draw_permanently_non_closable",
        "Lemma audit_p5_final_cpp01_gates_verified",
        "Lemma audit_p5_final_ecs_partition_complete",
        "Definition phase5_final_summary_audit_verdict",
        "Lemma phase5_final_summary_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_028: dict[str, list[str]] = {
    "Phase5FinalConsistencyCatalog.v": [
        "Definition p5_consistency_established",
        "Definition p5_consistency_proof018_ecs_open",
        "Theorem no_equivalence_claimed_in_phase5",
        "Theorem draw_prohibition_invariant_preserved",
        "Theorem cpp01_gate_stability_theorem",
        "Theorem pd_catalog_to_fvr_alignment_soundness",
        "Definition phase5_global_consistency",
        "Theorem phase5_global_consistency_holds",
    ],
    "Phase5FinalConsistencyAudit.v": [
        "Definition audit_p5_consistency_established",
        "Lemma audit_p5_consistency_ecs_aligned",
        "Lemma audit_cpp01_gate_stability_verified",
        "Lemma audit_pd_catalog_to_fvr_alignment",
        "Definition phase5_final_consistency_audit_verdict",
        "Lemma phase5_final_consistency_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_029: dict[str, list[str]] = {
    "Phase5FreezeCandidateCatalog.v": [
        "Definition p5_freeze_candidate_established",
        "Definition p5_freeze_candidate_freeze_link",
        "Definition p5_freeze_candidate_rpc_export",
        "Theorem phase5_freeze_candidate_no_equivalence",
        "Theorem phase5_freeze_candidate_draw_prohibited",
        "Theorem phase5_freeze_candidate_cpp01_stable",
        "Definition phase5_freeze_candidate_verdict",
        "Theorem phase5_freeze_candidate_verdict_holds",
    ],
    "Phase5FreezeCandidateAudit.v": [
        "Definition audit_p5_freeze_candidate_established",
        "Lemma audit_p5_freeze_candidate_snapshot_verified",
        "Lemma audit_p5_freeze_candidate_freeze_link",
        "Lemma audit_p5_freeze_candidate_rpc_unchanged",
        "Definition phase5_freeze_candidate_audit_verdict",
        "Lemma phase5_freeze_candidate_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_030: dict[str, list[str]] = {
    "PD4FCPP01RoadmapCatalog.v": [
        "Definition cpp01_gate_inventory",
        "Definition cpp01_dependency_graph",
        "Definition cpp01_readiness_graph",
        "Definition cpp01_closure_graph",
        "Definition cpp01_blocked_graph",
        "Definition cpp01_requirements_inventory",
        "Theorem cpp01_roadmap_closure_not_attempted",
        "Theorem cpp01_roadmap_dependency_soundness",
        "Definition pd4f_cpp01_roadmap_verdict",
        "Theorem pd4f_cpp01_roadmap_verdict_holds",
    ],
    "PD4FCPP01RoadmapAudit.v": [
        "Definition audit_cpp01_roadmap_established",
        "Lemma audit_cpp01_gate_inventory_complete",
        "Lemma audit_cpp01_roadmap_closure_not_attempted",
        "Lemma audit_cpp01_roadmap_dependency_soundness",
        "Lemma audit_cpp01_roadmap_blocked_implies_not_closable",
        "Definition pd4f_cpp01_roadmap_audit_verdict",
        "Lemma pd4f_cpp01_roadmap_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_031: dict[str, list[str]] = {
    "Phase5MainnetFreezeCatalog.v": [
        "Definition p5_phase5_paperwork_recorded",
        "Definition p5_mainnet_freeze_link",
        "Lemma p5_mainnet_freeze_snapshot_matches_candidate",
        "Theorem phase5_mainnet_freeze_draw_permanent",
        "Theorem phase5_mainnet_freeze_no_equivalence",
        "Theorem phase5_mainnet_freeze_cpp01_not_closed",
        "Definition phase5_mainnet_freeze_verdict",
        "Theorem phase5_mainnet_freeze_verdict_holds",
    ],
    "Phase5MainnetFreezeAudit.v": [
        "Definition audit_p5_phase5_paperwork_recorded",
        "Lemma audit_p5_mainnet_freeze_matches_candidate",
        "Lemma audit_p5_mainnet_freeze_draw_permanent",
        "Lemma audit_p5_mainnet_freeze_no_equivalence",
        "Definition phase5_mainnet_freeze_audit_verdict",
        "Lemma phase5_mainnet_freeze_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_032: dict[str, list[str]] = {
    "Phase5TerminalStatusCatalog.v": [
        "Definition p5_terminal_status_established",
        "Definition phase5_terminal_status",
        "Theorem phase5_terminal_status_holds",
        "Theorem cpp01_terminal_non_closable",
        "Theorem phase5_has_no_remaining_closable_path",
        "Definition phase5_terminal_verdict",
        "Theorem phase5_terminal_verdict_holds",
    ],
    "Phase5TerminalStatusAudit.v": [
        "Definition audit_p5_terminal_status_established",
        "Lemma audit_phase5_terminal_status_holds",
        "Lemma audit_cpp01_terminal_non_closable",
        "Lemma audit_p5_terminal_draw_permanently_impossible",
        "Definition phase5_terminal_audit_verdict",
        "Lemma phase5_terminal_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_PROOF_033: dict[str, list[str]] = {
    "PD4FCPP01NonClosableCatalog.v": [
        "Definition cpp01_nc_dependency_graph",
        "Definition cpp01_structural_closure_path_exists",
        "Definition cpp01_legal_closure_path_at_baseline",
        "Theorem cpp01_no_legal_closure_path",
        "Theorem draw_prohibition_blocks_global_equivalence",
        "Theorem spec007_blocks_equivalence_claim",
        "Theorem cpp01_permanently_non_closable",
        "Theorem phase5_terminal_status_implies_cpp01_non_closable",
        "Definition pd4f_cpp01_non_closability_verdict",
        "Theorem pd4f_cpp01_non_closability_verdict_holds",
    ],
    "PD4FCPP01NonClosableAudit.v": [
        "Definition audit_cpp01_nc_analysis_established",
        "Lemma audit_cpp01_no_legal_closure_path",
        "Lemma audit_cpp01_permanently_non_closable",
        "Lemma audit_phase5_terminal_implies_cpp01_non_closable",
        "Definition pd4f_cpp01_non_closability_audit_verdict",
        "Lemma pd4f_cpp01_non_closability_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_FVR_M: dict[str, list[str]] = {
    "DrawClosureImpossibilityCatalog.v": [
        "Inductive DrawClosureStatus",
        "Record DrawClosureCondition",
        "Definition current_draw_condition",
        "Definition Current_Draw_Architecture",
        "Definition classify_draw_closure_status",
        "Definition draw_closure_status",
        "Definition draw_missing_requirements",
        "Definition draw_impossibility_reasons",
        "Lemma draw_closure_status_well_defined",
        "Lemma draw_closure_implies_missing_encoder",
        "Lemma draw_closure_implies_missing_hash_kernel",
        "Lemma draw_cannot_reach_equivalence_closure",
        "Theorem draw_closure_impossibility",
        "Lemma draw_closure_does_not_establish_equivalence",
        "Lemma draw_closure_preserves_open_proof_debt",
    ],
    "DrawClosureImpossibilityAudit.v": [
        "Definition audit_draw_closure_status",
        "Definition audit_draw_missing_requirements",
        "Definition audit_draw_impossibility_reasons",
        "Lemma audit_draw_closure_impossibility",
        "Lemma audit_draw_cannot_reach_equivalence_closure",
        "Lemma audit_draw_closure_does_not_establish_equivalence",
        "Lemma audit_draw_closure_preserves_open_proof_debt",
        "Lemma draw_closure_impossibility_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_FVR_L: dict[str, list[str]] = {
    "EquivalenceClosureCatalog.v": [
        "Inductive EquivalenceClosureStatus",
        "Record EquivalenceClosureCondition",
        "Definition closure_allowed",
        "Definition equivalence_proof_permitted",
        "Definition build_closure_condition",
        "Definition classify_closure_status",
        "Definition check_equivalence_closure",
        "Definition equivalence_closure_catalog",
        "Theorem equivalence_closure_soundness",
        "Lemma closure_component_partition_complete",
        "Lemma closure_condition_well_defined",
        "Lemma equivalence_closure_does_not_establish_equivalence",
        "Lemma equivalence_closure_preserves_open_proof_debt",
        "Lemma sample_frozen_draw_prohibited",
        "Lemma sample_comparable_layers_present",
    ],
    "EquivalenceClosureAudit.v": [
        "Definition ecl_component_count",
        "Lemma closure_component_partition_complete",
        "Lemma audit_closure_condition_well_defined",
        "Lemma audit_equivalence_closure_soundness",
        "Lemma audit_closure_does_not_establish_equivalence",
        "Lemma audit_closure_preserves_open_proof_debt",
        "Lemma equivalence_closure_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_FVR_K: dict[str, list[str]] = {
    "GlobalCandidateEquivalenceCatalog.v": [
        "Record GlobalCandidateEquivalenceObservation",
        "Definition check_global_candidate_equivalence",
        "Fixpoint catalog_all_global_equivalence",
        "Definition global_equivalence_candidate_catalog",
        "Definition merge_ces",
        "Lemma check_global_candidate_equivalence_deterministic",
        "Lemma check_global_candidate_equivalence_total",
        "Lemma global_equivalence_status_well_defined",
        "Lemma global_equivalence_partition_complete",
        "Lemma global_equivalence_candidate_does_not_establish_equivalence",
        "Lemma global_equivalence_candidate_preserves_open_proof_debt",
        "Lemma sample_global_genesis_partial_oru",
        "Lemma sample_global_comparable_partial_oru",
        "Lemma sample_global_broken_blocked",
        "Lemma sample_global_draw_blocked_on_frozen_catalog",
    ],
    "GlobalCandidateEquivalenceAudit.v": [
        "Definition gcea_field_count",
        "Lemma gcea_partition_complete",
        "Lemma audit_check_global_candidate_equivalence_deterministic",
        "Lemma audit_check_global_candidate_equivalence_total",
        "Lemma audit_global_equivalence_does_not_establish_equivalence",
        "Lemma audit_global_equivalence_preserves_open_proof_debt",
        "Lemma global_equivalence_candidate_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_FVR_J: dict[str, list[str]] = {
    "RuntimeEquivalenceCandidateCatalog.v": [
        "Inductive CandidateEquivalenceStatus",
        "Record RuntimeWitness",
        "Record CoqWitnessRow",
        "Record CHASReport",
        "Record CandidateEquivalenceObservation",
        "Definition check_candidate_equivalence",
        "Fixpoint catalog_all_candidate_equivalence",
        "Definition runtime_equivalence_candidate_catalog",
        "Lemma check_candidate_equivalence_deterministic",
        "Lemma check_candidate_equivalence_total",
        "Lemma candidate_equivalence_status_well_defined",
        "Lemma candidate_equivalence_partition_complete",
        "Lemma runtime_equivalence_candidate_does_not_establish_equivalence",
        "Lemma runtime_equivalence_candidate_preserves_open_proof_debt",
        "Lemma sample_genesis_partial_supply",
        "Lemma sample_comparable_all_supply",
        "Lemma sample_broken_chain_blocked",
        "Record GoldenVectorRuntimeSnapshot",
        "Definition classify_golden_vector_closure_gate",
        "Definition golden_vector_runtime_catalog",
        "Definition golden_vector_equivalence_candidate_catalog",
        "Definition CoqEquivalenceCandidateExportToJson",
        "Lemma golden_vector_draw_closure_prohibited",
        "Lemma golden_vector_draw_closure_allowed_false",
        "Lemma golden_vector_closure_partition_complete",
        "Lemma golden_vector_equivalence_claimed_false",
    ],
    "RuntimeEquivalenceCandidateAudit.v": [
        "Definition reca_field_count",
        "Lemma reca_partition_complete",
        "Lemma audit_check_candidate_equivalence_deterministic",
        "Lemma audit_check_candidate_equivalence_total",
        "Lemma audit_runtime_equivalence_candidate_does_not_establish_equivalence",
        "Lemma audit_runtime_equivalence_candidate_preserves_open_proof_debt",
        "Lemma runtime_equivalence_candidate_audit_verdict_holds",
        "Definition reca_gv_count",
        "Lemma audit_golden_vector_draw_closure_allowed_false",
        "Lemma audit_golden_vector_ecs_alignment_holds",
        "Definition phase_d_closure_alignment_verdict",
        "Lemma phase_d_closure_alignment_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_J: dict[str, list[str]] = {
    "RuntimeWitnessRequirementCatalog.v": [
        "Inductive RuntimeWitnessField",
        "Inductive WitnessClosureLevel",
        "Record RuntimeWitnessRequirement",
        "Definition replay_cb_minimal_witnesses",
        "Definition reorg_minimal_witnesses",
        "Definition runtime_witness_catalog",
        "Definition total_required_fields",
        "Definition total_missing_fields",
        "Definition minimal_witness_sets_complete",
        "Lemma runtime_witness_catalog_nonempty",
        "Lemma runtime_witness_partition_complete",
        "Lemma runtime_witness_status_defined",
        "Lemma runtime_witness_preserves_open_proof_debt",
        "Lemma runtime_witness_does_not_establish_equivalence",
        "Lemma total_required_fields_value",
        "Lemma witness_targets_are_potentially_closable",
    ],
    "RuntimeWitnessRequirementAudit.v": [
        "Definition rwra_total_required",
        "Lemma rwra_total_required_value",
        "Lemma rwra_total_missing_value",
        "Lemma rwra_field_partition_complete",
        "Lemma rwra_minimal_witness_sets_complete",
        "Lemma audit_runtime_witness_partition_complete",
        "Lemma audit_runtime_witness_does_not_establish_equivalence",
        "Lemma audit_runtime_witness_preserves_open_proof_debt",
        "Lemma runtime_witness_requirement_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_I: dict[str, list[str]] = {
    "RuntimeSufficiencyGapCatalog.v": [
        "Inductive GapKind",
        "Inductive GapClosureStatus",
        "Record RuntimeGapRecord",
        "Definition runtime_gap_catalog",
        "Definition total_gap_count",
        "Definition components_with_closable_gap",
        "Definition components_with_nonclosable_gap",
        "Lemma runtime_gap_catalog_nonempty",
        "Lemma runtime_gap_partition_complete",
        "Lemma runtime_gap_status_defined",
        "Lemma runtime_gap_preserves_open_proof_debt",
        "Lemma runtime_gap_does_not_establish_equivalence",
        "Lemma total_gap_count_value",
        "Lemma draw_gap_nonclosable",
        "Lemma all_coq_eq_runtime_not_reachable",
    ],
    "RuntimeSufficiencyGapAudit.v": [
        "Definition rsga_total_gaps",
        "Lemma rsga_total_gaps_value",
        "Lemma rsga_closable_components_value",
        "Lemma rsga_nonclosable_components_value",
        "Lemma rsga_partition_complete",
        "Lemma audit_runtime_gap_partition_complete",
        "Lemma audit_runtime_gap_does_not_establish_equivalence",
        "Lemma audit_runtime_gap_preserves_open_proof_debt",
        "Lemma runtime_sufficiency_gap_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_H: dict[str, list[str]] = {
    "RuntimeArtifactReconstructionCatalog.v": [
        "Inductive ReconstructionStatus",
        "Inductive CanonicalFieldTag",
        "Record RuntimeReconstructionEntry",
        "Definition runtime_reconstruction_catalog",
        "Lemma runtime_reconstruction_catalog_nonempty",
        "Lemma runtime_reconstruction_partition_complete",
        "Lemma runtime_reconstruction_status_defined",
        "Lemma runtime_reconstruction_preserves_open_proof_debt",
        "Lemma runtime_reconstruction_does_not_establish_equivalence",
        "Lemma draw_not_reconstructable",
        "Lemma replay_reorg_cb_partially_reconstructable",
        "Lemma partial_count_value",
        "Lemma not_reconstructable_count_value",
    ],
    "RuntimeArtifactReconstructionAudit.v": [
        "Definition rra_total",
        "Lemma rra_total_value",
        "Lemma rra_partial_value",
        "Lemma rra_not_reconstructable_value",
        "Lemma rra_partition_complete",
        "Lemma audit_draw_not_reconstructable",
        "Lemma audit_runtime_reconstruction_does_not_establish_equivalence",
        "Lemma audit_runtime_reconstruction_preserves_open_proof_debt",
        "Lemma runtime_artifact_reconstruction_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_G: dict[str, list[str]] = {
    "RuntimeHashMappingResolution.v": [
        "Inductive ResolutionReason",
        "Record RuntimeMappingResolution",
        "Definition runtime_mapping_resolution",
        "Lemma runtime_mapping_resolution_matches_catalog",
        "Lemma runtime_mapping_resolution_total_entries",
        "Lemma runtime_mapping_resolution_partition_complete",
        "Lemma runtime_mapping_resolution_status_defined",
        "Lemma supply_eligibility_fundamentally_not_mappable",
        "Lemma runtime_mapping_resolution_preserves_open_proof_debt",
        "Lemma runtime_mapping_resolution_does_not_establish_equivalence",
        "Lemma res_partial_count_value",
        "Lemma res_not_mappable_count_value",
    ],
    "RuntimeHashMappingResolutionAudit.v": [
        "Definition rmra_total",
        "Lemma rmra_total_value",
        "Lemma rmra_partial_value",
        "Lemma rmra_not_mappable_value",
        "Lemma rmra_partition_complete",
        "Lemma audit_resolution_supply_eligibility_not_mappable",
        "Lemma audit_resolution_does_not_establish_equivalence",
        "Lemma audit_resolution_preserves_open_proof_debt",
        "Lemma runtime_mapping_resolution_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_F: dict[str, list[str]] = {
    "RuntimeHashMappingCatalog.v": [
        "Inductive RuntimeMappingStatus",
        "Record RuntimeMappingFlags",
        "Definition classify_runtime_mapping",
        "Record RuntimeMappingEntry",
        "Definition runtime_mapping_catalog",
        "Lemma runtime_mapping_catalog_nonempty",
        "Lemma runtime_mapping_total_entries",
        "Lemma runtime_mapping_partition_complete",
        "Lemma runtime_mapping_status_defined",
        "Lemma runtime_mapping_does_not_establish_equivalence",
        "Lemma runtime_mapping_preserves_open_proof_debt",
        "Lemma rmc_partial_count_value",
        "Lemma rmc_not_mappable_count_value",
    ],
    "RuntimeHashMappingAudit.v": [
        "Definition rm_audit_total",
        "Lemma rm_audit_total_value",
        "Lemma rm_audit_partial_value",
        "Lemma rm_audit_not_mappable_value",
        "Lemma rm_audit_partition_complete",
        "Lemma audit_runtime_mapping_does_not_establish_equivalence",
        "Lemma audit_runtime_mapping_preserves_open_proof_debt",
        "Lemma runtime_hash_mapping_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_E: dict[str, list[str]] = {
    "AntiEquivalenceSafety.v": [
        "Record AntiEquivalenceObservation",
        "Inductive AntiEquivalenceResult",
        "Definition is_cross_coq_external",
        "Definition check_anti_equivalence",
        "Fixpoint check_all_anti_equivalence",
        "Lemma anti_equivalence_deterministic",
        "Lemma anti_equivalence_total",
        "Lemma anti_equivalence_preserves_component",
        "Lemma anti_equivalence_detects_domain_gaps",
        "Lemma anti_equivalence_no_false_equivalence_leakage",
        "Lemma anti_equivalence_circularity_recorded",
        "Definition anti_equivalence_safety_framework_established",
        "Definition ae_coq_eq_runtime_established",
        "Definition ae_coq_eq_frozen_established",
        "Lemma ae_coq_eq_runtime_not_established",
        "Lemma ae_coq_eq_frozen_not_established",
        "Lemma all_ae_results_length",
    ],
    "AntiEquivalenceSafetyAudit.v": [
        "Definition ae_count_total",
        "Definition ae_aligned_count",
        "Definition ae_misaligned_count",
        "Definition ae_missing_count",
        "Definition ae_non_equivalence_count",
        "Lemma ae_count_total_value",
        "Lemma ae_aligned_count_value",
        "Lemma ae_misaligned_count_value",
        "Lemma ae_missing_count_value",
        "Lemma ae_non_equivalence_count_value",
        "Lemma audit_partition_complete",
        "Definition audit_no_equivalence_claims",
        "Lemma audit_no_equivalence_claims_holds",
        "Lemma anti_equivalence_safety_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_D: dict[str, list[str]] = {
    "CanonicalHashAlignmentBridge.v": [
        "Inductive Component",
        "Inductive HashSource",
        "Record HashObservation",
        "Inductive AlignmentResult",
        "Definition CanonicalHashAlignment",
        "Definition align_self",
        "Definition align_coq_vs_runtime",
        "Definition align_coq_vs_frozen",
        "Fixpoint align_all_observations",
        "Lemma alignment_deterministic",
        "Lemma alignment_total",
        "Lemma alignment_reflects_component_equality_only",
        "Lemma no_equivalence_leakage",
        "Lemma alignment_preserves_component_indexing",
        "Lemma alignment_domain_gap_is_structural_not_failure",
        "Definition canonical_hash_alignment_framework_established",
        "Definition coq_eq_runtime_established",
        "Definition coq_eq_frozen_established",
        "Lemma all_alignments_length",
    ],
    "CanonicalHashAlignmentAudit.v": [
        "Definition alignment_count_total",
        "Definition aligned_count",
        "Definition misaligned_count",
        "Definition missing_count",
        "Lemma alignment_count_total_value",
        "Lemma aligned_count_value",
        "Lemma misaligned_count_value",
        "Lemma missing_count_value",
        "Lemma alignment_partition_complete",
        "Definition alignment_does_not_imply_equivalence",
        "Lemma alignment_does_not_imply_equivalence_holds",
        "Lemma canonical_hash_alignment_audit_verdict_holds",
    ],
}

MODULE_EXPECTATIONS_5E_GG_C: dict[str, list[str]] = {
    "CanonicalStateHashModel.v": [
        "Definition CanonicalStateHash",
        "Definition state_hash_modulus",
        "Fixpoint state_hash",
        "Definition hash_replay_state",
        "Definition hash_reorg_state",
        "Definition hash_connectblock_state",
        "Definition hash_canonical_state",
        "Lemma state_hash_deterministic",
        "Lemma state_hash_total",
        "Lemma state_hash_bounded_or_well_defined",
        "Lemma hash_projection_consistency_replay",
        "Lemma hash_projection_consistency_reorg",
        "Lemma hash_projection_consistency_connectblock",
        "Definition canonical_state_hash_framework_established",
        "Definition hash_equivalence_to_runtime_established",
        "Definition hash_equivalence_to_frozen_established",
        "Lemma hash_equivalence_to_runtime_not_established",
        "Lemma hash_equivalence_to_frozen_not_established",
    ],
    "CanonicalStateHashAudit.v": [
        "Definition hash_encoder_count",
        "Lemma hash_encoder_count_value",
        "Definition all_hash_encoders_deterministic",
        "Lemma all_hash_encoders_deterministic_holds",
        "Definition all_hash_encoders_total",
        "Lemma all_hash_encoders_total_holds",
        "Definition all_hash_encoders_consistent_with_bytes",
        "Lemma all_hash_encoders_consistent_with_bytes_holds",
        "Lemma canonical_state_hash_audit_verdict_holds",
        "Lemma audit_hash_runtime_equivalence_not_established",
        "Lemma audit_hash_frozen_equivalence_not_established",
    ],
}

MODULE_EXPECTATIONS_5E_GG_B: dict[str, list[str]] = {
    "CanonicalBytesProjection.v": [
        "Definition CanonicalBytesEncoding",
        "Definition project_z_to_bytes",
        "Definition canonical_to_bytes",
        "Definition encode_replay_bytes",
        "Definition encode_reorg_bytes",
        "Definition encode_connectblock_bytes",
        "Definition encode_canonical_bytes",
        "Lemma replay_bytes_encoder_deterministic",
        "Lemma replay_bytes_encoder_total",
        "Lemma replay_bytes_encoder_nonempty",
        "Lemma reorg_bytes_encoder_deterministic",
        "Lemma reorg_bytes_encoder_total",
        "Lemma reorg_bytes_encoder_nonempty",
        "Lemma connectblock_bytes_encoder_deterministic",
        "Lemma connectblock_bytes_encoder_total",
        "Lemma connectblock_bytes_encoder_nonempty",
        "Lemma encode_replay_bytes_sample",
        "Lemma encode_reorg_bytes_sample",
        "Lemma encode_connectblock_bytes_sample",
        "Definition canonical_bytes_framework_established",
        "Lemma canonical_bytes_no_equivalence_claimed",
    ],
    "CanonicalBytesProjectionAudit.v": [
        "Definition canonical_bytes_encoder_count",
        "Lemma canonical_bytes_encoder_count_value",
        "Definition all_bytes_encoders_deterministic",
        "Lemma all_bytes_encoders_deterministic_holds",
        "Definition all_bytes_encoders_total",
        "Lemma all_bytes_encoders_total_holds",
        "Definition all_bytes_encoders_nonempty",
        "Lemma all_bytes_encoders_nonempty_holds",
        "Lemma canonical_bytes_projection_audit_verdict_holds",
        "Lemma audit_bytes_pd_4f_ob_02_open",
        "Lemma audit_bytes_pd_4f_cpp_01_open",
    ],
}

MODULE_EXPECTATIONS_5E_GG: dict[str, list[str]] = {
    "CanonicalStateEncoding.v": [
        "Inductive CanonicalState",
        "Definition encode_replay_state",
        "Definition encode_reorg_state",
        "Definition encode_connectblock_state",
        "Lemma replay_encoder_deterministic",
        "Lemma replay_encoder_total",
        "Lemma replay_encoder_nonempty",
        "Lemma reorg_encoder_deterministic",
        "Lemma reorg_encoder_total",
        "Lemma reorg_encoder_nonempty",
        "Lemma connectblock_encoder_deterministic",
        "Lemma connectblock_encoder_total",
        "Lemma connectblock_encoder_nonempty",
        "Definition canonical_encoding_framework_established",
        "Lemma canonical_encoding_no_equivalence_claimed",
    ],
    "CanonicalStateEncodingAudit.v": [
        "Definition canonical_encoder_count",
        "Lemma canonical_encoder_count_value",
        "Definition all_encoders_deterministic",
        "Lemma all_encoders_deterministic_holds",
        "Definition all_encoders_total",
        "Lemma all_encoders_total_holds",
        "Lemma canonical_state_encoding_audit_verdict_holds",
        "Lemma audit_pd_4f_ob_02_open",
        "Lemma audit_pd_4f_cpp_01_open",
    ],
}

MODULE_EXPECTATIONS_5E_E2E: dict[str, list[str]] = {
    "E2EEquivalenceCatalog.v": [
        "Inductive E2EComponent",
        "Inductive E2ESource",
        "Inductive E2EResult",
        "Inductive E2ECircularDependency",
        "Definition e2e_equivalence_matrix",
        "Lemma e2e_total_cells_value",
        "Lemma e2e_agree_count_value",
        "Lemma e2e_disagree_count_value",
        "Lemma e2e_missing_count_value",
        "Lemma e2e_partial_count_value",
        "Lemma e2e_counts_partition",
        "Lemma e2e_frozen_runtime_identities",
    ],
    "E2EEquivalenceAudit.v": [
        "Definition e2e_readiness_partial",
        "Lemma e2e_audit_no_full_agreement",
        "Lemma e2e_audit_disagreement_present",
        "Lemma e2e_audit_missing_present",
        "Lemma e2e_audit_circularity_tally",
        "Lemma e2e_audit_proof_debts_remain_open",
        "Lemma e2e_per_component_partial",
        "Lemma e2e_master_verdict_holds",
        "Lemma e2e_audit_no_global_equivalence",
    ],
}

MODULE_EXPECTATIONS_5E_SE: dict[str, list[str]] = {
    "SupplyEligibilityEquivalence.v": [
        "Definition coq_supply_after_trace",
        "Definition coq_eligibility_flags",
        "project_supply_after_trace",
        "project_eligibility_sigma",
        "Definition supply_frozen_side_result",
        "Lemma supply_frozen_side_is_missing",
        "Theorem supply_projection_structural_identity_reexport",
        "Lemma eligibility_row_all_resolved",
        "Lemma supply_projection_is_ps_partial",
        "Lemma eligibility_projection_is_ps_partial",
    ],
    "SupplyEligibilityEquivalenceCatalog.v": [
        "Record SupplyEligEquivCatalogEntry",
        "Definition se_equivalence_catalog",
        "Lemma se_catalog_missing_count_value",
        "Lemma se_catalog_partial_count_value",
        "Lemma all_eligibility_rows_resolved",
    ],
    "SupplyEligibilityEquivalenceAudit.v": [
        "Definition se_equivalence_marked_partial",
        "Lemma se_projection_layer_recorded_holds",
        "Lemma se_eligibility_identity_honest",
        "Lemma se_equivalence_verdict_holds",
        "Lemma se_audit_no_global_equivalence",
    ],
}

MODULE_EXPECTATIONS_5E_CB: dict[str, list[str]] = {
    "ConnectBlockEquivalence.v": [
        "Inductive ConnectBlockEquivKind",
        "Inductive ConnectBlockEquivResult",
        "Definition coq_cb_height",
        "Definition cb_projection_status",
        "Theorem cb_frozen_runtime_equivalence",
        "Lemma cb_projection_status_always_resolved",
        "Lemma cb_semantic_vs_frozen_all_disagree",
        "Lemma cb_semantic_vs_runtime_all_disagree",
        "Definition cb_projection_uses_identity_map",
        "Definition cb_projection_fabricates_hash",
    ],
    "ConnectBlockEquivalenceCatalog.v": [
        "Record ConnectBlockEquivCatalogEntry",
        "Definition cb_equivalence_catalog",
        "Lemma cb_catalog_resolved_count_value",
        "Lemma cb_catalog_disagreement_count_value",
        "Lemma all_totality_rows_resolved",
        "Lemma all_semantic_vs_frozen_rows_disagree",
        "Lemma all_semantic_vs_runtime_rows_disagree",
    ],
    "ConnectBlockEquivalenceAudit.v": [
        "Definition cb_equivalence_marked_partial",
        "Lemma cb_projection_layer_recorded_holds",
        "Lemma cb_encoder_does_not_fabricate_hash",
        "Lemma cb_equivalence_verdict_holds",
        "Lemma cb_audit_no_global_equivalence",
    ],
}

MODULE_EXPECTATIONS_5E_REORG: dict[str, list[str]] = {
    "ReorgEquivalence.v": [
        "Inductive ReorgEquivKind",
        "Inductive ReorgEquivResult",
        "Definition coq_reorg_height",
        "Definition reorg_projection_status",
        "Theorem reorg_frozen_runtime_equivalence",
        "Lemma reorg_projection_status_always_resolved",
        "Lemma reorg_semantic_vs_frozen_all_disagree",
        "Lemma reorg_semantic_vs_runtime_all_disagree",
        "Definition reorg_projection_uses_identity_map",
    ],
    "ReorgEquivalenceCatalog.v": [
        "Record ReorgEquivCatalogEntry",
        "Definition reorg_equivalence_catalog",
        "Lemma reorg_catalog_resolved_count_value",
        "Lemma reorg_catalog_disagreement_count_value",
        "Lemma all_totality_rows_resolved",
        "Lemma all_semantic_vs_frozen_rows_disagree",
        "Lemma all_semantic_vs_runtime_rows_disagree",
    ],
    "ReorgEquivalenceAudit.v": [
        "Definition reorg_equivalence_marked_partial",
        "Lemma reorg_projection_layer_recorded_holds",
        "Lemma reorg_equivalence_verdict_holds",
        "Lemma reorg_audit_no_global_equivalence",
    ],
}

MODULE_EXPECTATIONS_5E_REPLAY: dict[str, list[str]] = {
    "ReplayEquivalence.v": [
        "Inductive ReplayEquivKind",
        "Inductive ReplayEquivResult",
        "Definition coq_replay_height",
        "Definition replay_projection_status",
        "Theorem replay_frozen_runtime_equivalence",
        "Lemma replay_projection_status_always_resolved",
        "Lemma replay_semantic_vs_frozen_all_disagree",
        "Lemma replay_semantic_vs_runtime_all_disagree",
        "Definition replay_projection_uses_identity_map",
    ],
    "ReplayEquivalenceCatalog.v": [
        "Record ReplayEquivCatalogEntry",
        "Definition replay_equivalence_catalog",
        "Lemma replay_catalog_resolved_count_value",
        "Lemma replay_catalog_disagreement_count_value",
        "Lemma all_totality_rows_resolved",
        "Lemma all_semantic_vs_frozen_rows_disagree",
        "Lemma all_semantic_vs_runtime_rows_disagree",
    ],
    "ReplayEquivalenceAudit.v": [
        "Definition replay_equivalence_marked_partial",
        "Lemma replay_projection_layer_recorded_holds",
        "Lemma replay_equivalence_verdict_holds",
        "Lemma replay_audit_no_global_equivalence",
    ],
}

MODULE_EXPECTATIONS_5E_DRAW: dict[str, list[str]] = {
    "DrawEquivalence.v": [
        "Definition draw_coq_projected_txid",
        "Definition draw_runtime_shaped_txid",
        "Theorem draw_coq_runtime_shaped_equivalence",
        "Theorem draw_frozen_live_runtime_equivalence",
        "Lemma any_golden_row_matches_frozen_false",
        "Lemma draw_cross_domain_all_disagree",
        "Definition draw_projection_uses_identity_map",
    ],
    "DrawEquivalenceCatalog.v": [
        "Definition draw_equivalence_catalog",
        "Lemma draw_catalog_resolved_count_value",
        "Lemma draw_catalog_disagreement_count_value",
        "Lemma all_golden_projection_rows_resolved",
        "Lemma all_cross_domain_rows_disagree",
    ],
    "DrawEquivalenceAudit.v": [
        "Definition draw_equivalence_marked_partial",
        "Lemma draw_projection_layer_verified_holds",
        "Lemma draw_equivalence_verdict_holds",
        "Lemma draw_audit_no_global_equivalence",
    ],
}

DOC_EXPECTATIONS_5E: dict[str, list[str]] = {
    "EQUIVALENCE_ARCHITECTURE_AUDIT.md": [
 "",
        "SCAFFOLD ONLY",
        "no equivalence claimed",
        "resolved catalog cells",
        "**0**",
        "PD-4F-OB-02",
    ],
    "DRAW_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "Draw equivalence projection verified",
        "other components remain OPEN/PARTIAL",
        "14/14",
        "frozen = live-runtime",
        "PD-4F-OB-02",
    ],
    "REPLAY_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "Replay equivalence projections recorded",
        "no equivalence claimed",
        "frozen = runtime",
        "Disagreement",
        "PD-4F-OB-02",
    ],
    "REORG_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "Reorg equivalence projections verified",
        "disagreements documented",
        "frozen = runtime",
        "Disagreement",
        "PD-4F-OB-02",
    ],
    "CONNECTBLOCK_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "ConnectBlock projections verified",
        "no equivalence claimed",
        "frozen = runtime",
        "does not fabricate",
        "Supply still missing",
        "PD-4F-OB-02",
    ],
    "SUPPLY_ELIGIBILITY_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "Supply and eligibility projections verified",
        "no equivalence claims",
        "PS_Partial",
        "Missing",
        "identity mapping",
        "PD-4F-OB-03",
    ],
    "E2E_EQUIVALENCE_AUDIT.md": [
 "",
        "PARTIAL",
        "E2E projections combined",
        "no equivalence claims asserted",
        "readiness is partial",
        "6",
        "Disagree",
        "Missing",
        "PD-4F-OB-02",
        "PD-4F-OB-03",
    ],
    "RUNTIME_HASH_MAPPING_FEASIBILITY.md": [
 "",
        "ESTABLISHED",
        "RMS_PartiallyMappable",
        "RMS_NotMappable",
        "no equivalence established",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_HASH_MAPPING_RESOLUTION.md": [
 "",
        "ESTABLISHED",
        "RMS_PartiallyMappable",
        "RMS_NotMappable",
        "no equivalence asserted",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "Supply",
        "Eligibility",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_ARTIFACT_RECONSTRUCTION_FEASIBILITY.md": [
 "",
        "ESTABLISHED",
        "RCS_PartiallyReconstructable",
        "RCS_NotReconstructable",
        "CanonicalStateHash",
        "no equivalence asserted",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "Draw",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_SUFFICIENCY_GAP_ANALYSIS.md": [
 "",
        "ESTABLISHED",
        "GCS_PotentiallyClosable",
        "GCS_NonClosable",
        "total_gap_count",
        "no equivalence asserted",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "Draw",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_WITNESS_REQUIREMENT_ANALYSIS.md": [
 "",
        "ESTABLISHED",
        "WCL_MissingWitnesses",
        "RWF_TraceHistory",
        "RWF_HashKernelOutput",
        "total_required_fields",
        "no equivalence asserted",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "Replay",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_ALIGNMENT_ANALYSIS.md": [
 "",
        "ESTABLISHED",
        "FAS_PartiallyAligned",
        "CAS_PartialAlignment",
        "CAS_FundamentallyBlocked",
        "no equivalence asserted",
        "Coq \u2261 Runtime",
        "NOT ESTABLISHED",
        "HashKernelOutput",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_WITNESS_INTEGRATION.md": [
 "",
        "ESTABLISHED",
        "IFS_Partial",
        "IFS_Missing",
        "FAA_DerivedFromCoq",
        "FAA_RequiresShaKernel",
        "CAS_PartialAlignment",
        "fundamentally blocked",
        "NOT ESTABLISHED",
        "Coq \u2261 Runtime",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_ALIGNMENT_INTEGRATION.md": [
 "",
        "ESTABLISHED",
        "IAS_Partial",
        "IAS_Missing",
        "IAS_FundamentallyBlocked",
        "CAS_PartialAlignment",
        "fundamentally blocked",
        "NOT ESTABLISHED",
        "Coq \u2261 Runtime",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
    "RUNTIME_HASH_ALIGNMENT.md": [
 "",
        "ESTABLISHED",
        "CHAS_Partial",
        "CHAS_Missing",
        "CHAS_FundamentallyBlocked",
        "hash_replay_state",
        "fundamentally blocked",
        "NOT ESTABLISHED",
        "Coq \u2261 Runtime",
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
    ],
}

# Required release docs under docs/formal/release/.
RELEASE_DOCS_DIR = REPO_ROOT / "docs" / "formal" / "release"
RELEASE_DOC_EXPECTATIONS: dict[str, list[str]] = {
    "FINAL_FORMAL_VERIFICATION_REPORT.md": [
        "## 1. Scope of verification",
        "## 2. Components covered",
        "## 3. Projection framework summary",
        "## 4. Audit framework summary",
        "## 5. Readiness framework summary",
        "## 6. Circular dependency summary",
        "## 7. Remaining proof debt summary",
        "formal_verification_ready",
    ],
    "FINAL_PROOF_MAP.md": [
        "Phase4FConsensus",
        "Phase4GRefinement",
        "FormalVerificationCertification.v",
        "ADMIT-PHASE4F",
    ],
    "FINAL_PROOF_DEBT_STATUS.md": [
        "PD-4F-OB-02",
        "PD-4F-CPP-01",
        "PD-4F-OB-03",
        "OPEN",
        "Closure requirements",
    ],
    "FINAL_RELEASE_VERDICT.md": [
        "Phase-4F / Phase-4G",
        "READY_FOR_RELEASE",
        "PARTIAL",
        "NOT_ESTABLISHED",
        "Open Proof Debt",
        "formal_verification_ready",
    ],
}

DOC_EXPECTATIONS: dict[str, list[str]] = {
    "CONSENSUS_REFINEMENT_MAP.md": ["## 1. End-to-end consensus machine", "**MATCH**"],
    "ORACLE_EQUIVALENCE_MATRIX.md": ["## 1. Summary", "**BLOCKED**"],
    "GOLDEN_VECTOR_COVERAGE_AUDIT.md": ["## 1. Aggregate coverage", "**MATCH**"],
    "CONSENSUS_CLAIM_BOUNDARY.md": ["## 1. Allowed claims", "## 2. Not allowed claims"],
    "PHASE4F_READINESS_VERDICT.md": ["## 1. Verdict", "## 5. Mandatory conditions"],
    "PHASE4F_TRACE_MODEL.md": ["## 1. Layer stack", "Coq Trace (Phase-4F)"],
    "GOLDEN_VECTOR_EXECUTION_MATRIX.md": ["## 1. Summary matrix", "PHASE4E_CONNECTBLOCK"],
    "CONSENSUS_EXECUTION_BOUNDARY.md": [
        "PHASE4F does not yet establish Coq",
        "No golden vector has been truly verified",
        "## 2. What Phase-4F CAN currently claim",
        "## 3. What Phase-4F CANNOT claim",
    ],
    "ORACLE_BRIDGE_MATRIX.md": ["## 1. Matrix", "ConnectBlock", "RES Runtime"],
    "ORACLE_CIRCULARITY_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —"],
    "ORACLE_BRIDGE_SOUNDNESS.md": ["Proven", "Observed", "Imported", "Assumed", "Unknown"],
    "PROOF_DEBT_REGISTER.md": ["PD-4F-OB-01", "PD-4F-OB-02", "PD-4F-OB-03"],
    "RUNTIME_EVIDENCE_BRIDGE_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "PD-4F-OB-01"],
    "TRACE_AGREEMENT_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**NO.**"],
    "DRAW_REPRESENTATION_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "winner_projection_sound"],
    "FROZEN_CPP_ORACLE_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "PD-4F-CPP-01"],
    "FROZEN_CPP_AGREEMENT_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**NO.**"],
    "FROZEN_CPP_TRACE_COMPARISON_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**NO.**"],
    "SEMANTIC_DERIVATION_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**NO.**"],
    "SEMANTIC_VS_FROZEN_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**No.**"],
    "SEMANTIC_VS_LIVE_RUNTIME_AUDIT.md": ["Q1 —", "Q2 —", "Q3 —", "Q4 —", "**No.**"],
    "SEMANTIC_VS_E2E_AUDIT.md": ["## Final unified statistics", "PARTIAL", "OPEN"],
}

GENERATOR = REPO_ROOT / "formal_verification" / "tools" / "generate_phase4f_golden_traces.py"
GOLDEN_JSON = REPO_ROOT / "formal_verification" / "phase3_golden_vectors.json"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def check_forbidden_imports() -> list[str]:
    """Phase-4F skeleton purity gate: no SHA-256 / ConvergeRoot / legacy
    Execution / ConnectBlock / SigmaCR01 / RigorHash modules may be required
    from Phase4FConsensus. Phase4GRefinement and Phase5Equivalence are
    explicitly allowed to reference Phase-4G state-encoding modules whose
    file names share these substrings."""
    errors: list[str] = []
    vfiles: list[Path] = list(COQ_DIR.glob("*.v"))
    for vfile in vfiles:
        for line in read_text(vfile).splitlines():
            stripped = line.strip()
            if not (
                stripped.startswith("Require")
                or stripped.startswith("From ")
                or stripped.startswith("Import ")
            ):
                continue
            for forbidden in FORBIDDEN_IMPORTS:
                if forbidden in line:
                    errors.append(
                        f"{vfile.name}: forbidden import of {forbidden}: {stripped}"
                    )
    return errors


def check_definitions() -> list[str]:
    errors: list[str] = []
    for filename, needles in MODULE_EXPECTATIONS.items():
        path = COQ_DIR / filename
        if not path.is_file():
            errors.append(f"missing module file: {filename}")
            continue
        text = read_text(path)
        for needle in needles:
            if needle not in text:
                errors.append(f"{filename}: missing `{needle}`")
    for label, coq_dir, expectations in (
        ("Phase4GRefinement", COQ_DIR_4G, MODULE_EXPECTATIONS_4G),
        ("Phase4GRefinement", COQ_DIR_4G, MODULE_EXPECTATIONS_4G_CLOSURE),
        ("Phase4GRefinement", COQ_DIR_4G, MODULE_EXPECTATIONS_4G_FINAL),
        ("Phase4GRefinement", COQ_DIR_4G, MODULE_EXPECTATIONS_4G_CERT),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_DRAW),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_REPLAY),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_REORG),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_CB),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_SE),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_E2E),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_B),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_C),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_D),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_E),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_F),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_G),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_H),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_I),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_J),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_K),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_L),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_M),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_GG_N),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_FVR_J),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_FVR_K),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_FVR_L),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_FVR_M),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_FVR_N),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_019),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_020),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_021),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_022),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_023),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_024),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_025),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_026),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_027),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_028),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_029),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_030),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_031),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_032),
        ("Phase5Equivalence", COQ_DIR_5E, MODULE_EXPECTATIONS_5E_PROOF_033),
    ):
        for filename, needles in expectations.items():
            path = coq_dir / filename
            if not path.is_file():
                errors.append(f"missing module file: {label}/{filename}")
                continue
            text = read_text(path)
            for needle in needles:
                if needle not in text:
                    errors.append(f"{label}/{filename}: missing `{needle}`")
            if "Admitted" in text:
                errors.append(f"{label}/{filename}: introduces an `Admitted` proof")
    return errors


def check_coq_project() -> list[str]:
    """RFC-019 Phase 5: these modules were git-mv'd to archive/lottery.

    The live _CoqProject must not list them. The files themselves are checked
    via COQ_DIR / COQ_DIR_4G / COQ_DIR_5E, which now point at the archive.
    """
    errors: list[str] = []
    if not COQ_PROJECT.is_file():
        return ["missing _CoqProject"]
    proj = read_text(COQ_PROJECT)
    prefixes = ("Phase4FConsensus/", "Phase4GRefinement/", "Phase5Equivalence/")
    for line in proj.splitlines():
        s = line.strip()
        if any(s.startswith(p) for p in prefixes):
            errors.append(f"_CoqProject still lists archived module: {s}")
    return errors



def check_generated_data() -> list[str]:
    errors: list[str] = []
    data_v = COQ_DIR / "ConsensusGoldenVectorsData.v"
    if not data_v.is_file():
        errors.append("missing ConsensusGoldenVectorsData.v (run generate_phase4f_golden_traces.py)")
        return errors
    text = read_text(data_v)
    if "generated_trace_case_count : nat := 370" not in text:
        errors.append("ConsensusGoldenVectorsData.v: expected generated_trace_case_count := 370")
    if not GENERATOR.is_file():
        errors.append(f"missing generator: {GENERATOR}")
    if not GOLDEN_JSON.is_file():
        errors.append(f"missing golden JSON: {GOLDEN_JSON}")
    return errors


def check_docs() -> list[str] | None:
    """None means SKIP: docs/formal/ is wholly absent from this tree.

    The public tree historically omitted the ~50 stage-gate audit markdowns.
    When the tree is wholly absent, SKIP. When a partial tree is present
    (e.g. RFC-017 supply audits only), enforce section checklists solely for
    Phase-4F / release / 5E pages that exist — do not require inventing the
    full historical doc set.
    """
    formal_root = REPO_ROOT / "docs" / "formal"
    if not formal_root.is_dir() or not any(formal_root.rglob("*.md")):
        return None
    errors: list[str] = []
    checked = 0
    for name, sections in DOC_EXPECTATIONS.items():
        path = DOC_DIR / name
        if not path.is_file():
            continue
        checked += 1
        text = read_text(path)
        for section in sections:
            if section not in text:
                errors.append(f"{name}: missing `{section}`")
    for name, sections in RELEASE_DOC_EXPECTATIONS.items():
        path = RELEASE_DOCS_DIR / name
        if not path.is_file():
            continue
        checked += 1
        text = read_text(path)
        for section in sections:
            if section not in text:
                errors.append(f"release/{name}: missing `{section}`")
    for name, sections in DOC_EXPECTATIONS_5E.items():
        path = DOC_DIR_5E / name
        if not path.is_file():
            continue
        checked += 1
        text = read_text(path)
        for section in sections:
            if section not in text:
                errors.append(f"phase5_equivalence/{name}: missing `{section}`")
    if checked == 0:
        # Formal tree exists for other phases (e.g. RFC-017); nothing for 4F.
        return []
    return errors


def main() -> int:
    all_errors: list[str] = []
    all_errors.extend(check_coq_project())
    all_errors.extend(check_forbidden_imports())
    all_errors.extend(check_definitions())
    all_errors.extend(check_generated_data())
    docs = check_docs()
    if docs is None:
        print("SKIP: docs/formal/ absent (internal stage-gate audits; not part of this tree)")
    else:
        all_errors.extend(docs)

    if all_errors:
        print("FAIL: Phase4FConsensus structural validation")
        for err in all_errors:
            print(f"  - {err}")
        return 1

    print("PASS: Phase4FConsensus structural validation (PHASE-4F 001A/001B/001C/001D)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
