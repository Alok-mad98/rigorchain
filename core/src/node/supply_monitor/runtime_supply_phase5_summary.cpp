// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_phase5_summary.h>

#include <node/supply_monitor/runtime_supply_equivalence_closure.h>
#include <node/supply_monitor/runtime_supply_global_candidate_equivalence.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>
#include <node/supply_monitor/runtime_supply_witness_coq_adapter.h>
#include <node/supply_monitor/runtime_supply_witness_equivalence_candidate.h>
#include <node/supply_monitor/runtime_supply_coq_chas_adapter.h>

#include <chain.h>
#include <univalue.h>

namespace Consensus {

namespace {

constexpr const char* kFreezeProtocolLink =
    "MAINNET_PROTOCOL_FREEZE_DECLARATION.md/";
constexpr const char* kFreezeNote =
    "Phase-5 formal snapshot; constitutional freeze unchanged.";

Phase5LayerSummary MakeLayer(const char* task,
                             const char* name,
                             Phase5LayerStatus status,
                             bool runtime_present)
{
    Phase5LayerSummary layer;
    layer.task_id = task;
    layer.layer_name = name;
    layer.status = status;
    layer.layer_established = true;
    layer.coq_eq_runtime = false;
    layer.runtime_present = runtime_present;
    return layer;
}

} // namespace

const char* Phase5LayerStatusToString(const Phase5LayerStatus status)
{
    switch (status) {
    case Phase5LayerStatus::Established:
        return "ESTABLISHED";
    case Phase5LayerStatus::Active:
        return "ACTIVE";
    case Phase5LayerStatus::Partial:
        return "PARTIAL";
    case Phase5LayerStatus::Blocked:
        return "BLOCKED";
    case Phase5LayerStatus::Impossible:
        return "IMPOSSIBLE";
    }
    return "ACTIVE";
}

std::optional<Phase5SummaryExport> ResolvePhase5SummaryAt(const CBlockIndex* pindex)
{
    if (pindex == nullptr) {
        return std::nullopt;
    }

    Phase5SummaryExport exp;
    exp.height = pindex->nHeight;
    exp.equivalence_claimed = false;
    exp.freeze_protocol_link = kFreezeProtocolLink;
    exp.freeze_note = kFreezeNote;

    const std::optional<RuntimeSupplyWitness> witness =
        GetRuntimeSupplyWitness(pindex);
    const std::optional<RuntimeSupplyWitnessCoqCatalogRow> catalog =
        ResolveWitnessCoqCatalogRowAt(pindex);
    const std::optional<RuntimeSupplyChasAlignmentReport> chas =
        ResolveChasAlignmentAt(pindex);
    const std::optional<WitnessEquivalenceCandidate> candidate =
        ResolveWitnessEquivalenceCandidateAt(pindex);
    const std::optional<GlobalCandidateEquivalenceObservation> global =
        ResolveGlobalCandidateEquivalenceAt(pindex);
    const std::optional<EquivalenceClosureResult> closure =
        ResolveEquivalenceClosureAt(pindex);

    const bool has_witness = witness.has_value();
    const bool has_catalog = catalog.has_value();
    const bool has_chas = chas.has_value();
    const bool has_candidate = candidate.has_value();
    const bool has_global = global.has_value();
    const bool invariants_passed =
        witness.has_value() && witness->invariants_passed &&
        (!catalog.has_value() || catalog->invariants_passed);

    exp.layers.push_back(MakeLayer("FVR-B", "RuntimeSupply",
                                   Phase5LayerStatus::Established, has_witness));
    exp.layers.push_back(MakeLayer("FVR-E", "RuntimeInvariants",
                                   invariants_passed ? Phase5LayerStatus::Active
                                                     : Phase5LayerStatus::Blocked,
                                   invariants_passed));
    exp.layers.push_back(MakeLayer("FVR-F", "RuntimeWitnessChain",
                                   has_witness ? Phase5LayerStatus::Active
                                               : Phase5LayerStatus::Partial,
                                   has_witness));
    exp.layers.push_back(MakeLayer("FVR-G", "RuntimeCoqCatalog",
                                   has_catalog ? Phase5LayerStatus::Active
                                               : Phase5LayerStatus::Partial,
                                   has_catalog));
    exp.layers.push_back(MakeLayer("FVR-H", "RuntimeChasAlignment",
                                   has_chas ? Phase5LayerStatus::Active
                                            : Phase5LayerStatus::Partial,
                                   has_chas));
    exp.layers.push_back(MakeLayer("FVR-J", "CandidateEquivalence",
                                   has_candidate ? Phase5LayerStatus::Active
                                                 : Phase5LayerStatus::Partial,
                                   has_candidate));
    exp.layers.push_back(MakeLayer("FVR-K", "GlobalCandidateEquivalence",
                                   has_global ? Phase5LayerStatus::Active
                                              : Phase5LayerStatus::Partial,
                                   has_global));
    exp.layers.push_back(MakeLayer("FVR-L", "EquivalenceClosure",
                                   closure.has_value() ? Phase5LayerStatus::Active
                                                       : Phase5LayerStatus::Partial,
                                   closure.has_value()));

    if (closure.has_value()) {
        exp.closure_status = EquivalenceClosureStatusToString(closure->status);
        exp.closure_allowed = closure->closure_allowed;
    }

    return exp;
}

UniValue Phase5SummaryExportToJson(const Phase5SummaryExport& exp)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("task", "FVR-N");
    obj.pushKV("height", exp.height);
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("pd_4f_ob_02", "OPEN");
    obj.pushKV("pd_4f_cpp_01", "OPEN");
    obj.pushKV("phase5_summary_catalog_established", true);
    obj.pushKV("freeze_protocol_link", exp.freeze_protocol_link);
    obj.pushKV("freeze_note", exp.freeze_note);
    obj.pushKV("closure_status", exp.closure_status);
    obj.pushKV("closure_allowed", exp.closure_allowed);

    UniValue layers(UniValue::VARR);
    for (const Phase5LayerSummary& layer : exp.layers) {
        UniValue entry(UniValue::VOBJ);
        entry.pushKV("ple_task_id", layer.task_id);
        entry.pushKV("ple_layer_name", layer.layer_name);
        entry.pushKV("ple_status", Phase5LayerStatusToString(layer.status));
        entry.pushKV("ple_layer_established", layer.layer_established);
        entry.pushKV("ple_coq_eq_runtime", false);
        entry.pushKV("runtime_present", layer.runtime_present);
        layers.push_back(std::move(entry));
    }
    obj.pushKV("phase5_layer_catalog", std::move(layers));

    return obj;
}

std::optional<Phase5SummaryFinalExport> ResolvePhase5SummaryFinalAt(const CBlockIndex* pindex)
{
    const std::optional<Phase5SummaryExport> base = ResolvePhase5SummaryAt(pindex);
    if (!base.has_value()) {
        return std::nullopt;
    }

    Phase5SummaryFinalExport exp;
    exp.base = *base;
    exp.pd13_status = "CLOSED";
    exp.pd14_status = "CLOSED";
    exp.pd15_status = "CLOSED";
    exp.pd4f_ob02_status = "READY";
    exp.pd4f_cpp01_status = "OPEN";
    exp.cpp01_runtime_alignment_ready = false;
    exp.cpp01_vector_alignment_ready = false;
    exp.cpp01_coq_alignment_ready = false;
    exp.cpp01_candidate_equivalence_ready = false;
    exp.candidate_equivalence_evidence_ready = false;
    exp.candidate_equivalence_runtime_ready = false;
    exp.golden_vector_total = 12;
    exp.golden_vector_materialized = 9;
    exp.ecs_open_count = 3;
    exp.ecs_partial_count = 7;
    exp.ecs_prohibited_count = 2;
    return exp;
}

UniValue Phase5SummaryFinalExportToJson(const Phase5SummaryFinalExport& exp)
{
    UniValue obj = Phase5SummaryExportToJson(exp.base);
    obj.pushKV("task", "PROOF-027");
    obj.pushKV("pd13_status", exp.pd13_status);
    obj.pushKV("pd14_status", exp.pd14_status);
    obj.pushKV("pd15_status", exp.pd15_status);
    obj.pushKV("pd_4f_ob_02", exp.pd4f_ob02_status);
    obj.pushKV("pd_4f_cpp_01", exp.pd4f_cpp01_status);
    obj.pushKV("cpp01_runtime_alignment_ready", exp.cpp01_runtime_alignment_ready);
    obj.pushKV("cpp01_vector_alignment_ready", exp.cpp01_vector_alignment_ready);
    obj.pushKV("cpp01_coq_alignment_ready", exp.cpp01_coq_alignment_ready);
    obj.pushKV("cpp01_candidate_equivalence_ready", exp.cpp01_candidate_equivalence_ready);
    obj.pushKV("candidate_equivalence_evidence_ready", exp.candidate_equivalence_evidence_ready);
    obj.pushKV("candidate_equivalence_runtime_ready", exp.candidate_equivalence_runtime_ready);
    obj.pushKV("golden_vector_total", exp.golden_vector_total);
    obj.pushKV("golden_vector_materialized", exp.golden_vector_materialized);
    obj.pushKV("ecs_open_count", exp.ecs_open_count);
    obj.pushKV("ecs_partial_count", exp.ecs_partial_count);
    obj.pushKV("ecs_prohibited_count", exp.ecs_prohibited_count);
    obj.pushKV("phase5_final_catalog_established", true);
    return obj;
}

} // namespace Consensus
