// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_equivalence_closure.h>

#include <chain.h>
#include <univalue.h>

#include <algorithm>

namespace Consensus {

namespace {

constexpr const char* DRAW_BLOCKED_REASON = "DRAW-CES-FundamentallyBlocked";

constexpr bool kCandidateEquivalenceLayerEstablished = true;
constexpr bool kGlobalCandidateEquivalenceLayerEstablished = true;
constexpr bool kAlignmentIntegrationLayerEstablished = true;

bool HasReason(const std::vector<std::string>& reasons, const std::string& reason)
{
    return std::find(reasons.begin(), reasons.end(), reason) != reasons.end();
}

bool RuntimeWitnessChainOk(const RuntimeSupplyWitness& witness,
                           const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    if (witness.height < 0) {
        return false;
    }
    if (catalog.height == 0) {
        return catalog.delta.witness_chain_step_valid;
    }
    return catalog.delta.parent_witness_present && catalog.delta.witness_chain_step_valid;
}

bool ChasAlignmentOk(const RuntimeSupplyChasAlignmentReport& chas)
{
    return chas.chas_oru_state.status != CanonicalHashAlignmentStatus::FundamentallyBlocked &&
           chas.chas_res_state.status != CanonicalHashAlignmentStatus::FundamentallyBlocked;
}

bool CoqCatalogAlignmentOk(const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    return kAlignmentIntegrationLayerEstablished && catalog.height >= 0;
}

bool IsFundamentallyProhibited(const WitnessEquivalenceCandidate& candidate,
                               const RuntimeSupplyChasAlignmentReport& chas)
{
    if (!ChasAlignmentOk(chas)) {
        return true;
    }
    if (candidate.invariant_status == CandidateEquivalenceStatus::FundamentallyBlocked) {
        return true;
    }
    if (candidate.witness_chain_status == CandidateEquivalenceStatus::FundamentallyBlocked) {
        return true;
    }
    if (HasReason(candidate.blocking_reasons, DRAW_BLOCKED_REASON)) {
        return true;
    }
    return false;
}

bool AllClosureLayersPresent(const EquivalenceClosureCondition& condition)
{
    return condition.has_runtime_witness_chain &&
           condition.has_invariant_pass &&
           condition.has_candidate_equivalence_layer &&
           condition.has_global_candidate_equivalence_layer &&
           condition.has_chas_alignment_layer &&
           condition.has_coq_catalog_alignment_layer;
}

void AppendUnique(std::vector<std::string>& layers, const std::string& layer)
{
    if (std::find(layers.begin(), layers.end(), layer) == layers.end()) {
        layers.push_back(layer);
    }
}

EquivalenceClosureStatus ClassifyClosureStatus(const EquivalenceClosureCondition& condition,
                                              const WitnessEquivalenceCandidate& candidate,
                                              const RuntimeSupplyChasAlignmentReport& chas)
{
    if (IsFundamentallyProhibited(candidate, chas)) {
        return EquivalenceClosureStatus::Prohibited;
    }
    if (ClosureAllowed(condition)) {
        return EquivalenceClosureStatus::Closable;
    }
    if (AllClosureLayersPresent(condition)) {
        return EquivalenceClosureStatus::PartiallyClosable;
    }
    return EquivalenceClosureStatus::Open;
}

void PopulateLayerLists(EquivalenceClosureResult& result)
{
    const EquivalenceClosureCondition& c = result.condition;
    if (c.has_runtime_witness_chain) {
        AppendUnique(result.active_layers, "FVR-F-runtime-witness-chain");
    } else {
        AppendUnique(result.missing_layers, "FVR-F-runtime-witness-chain");
    }
    if (c.has_invariant_pass) {
        AppendUnique(result.active_layers, "FVR-E-invariants");
    } else {
        AppendUnique(result.missing_layers, "FVR-E-invariants");
    }
    if (c.has_coq_catalog_alignment_layer) {
        AppendUnique(result.active_layers, "FVR-G-coq-catalog");
        AppendUnique(result.active_layers, "FVR-M-alignment-integration");
    } else {
        AppendUnique(result.missing_layers, "FVR-G-coq-catalog");
        AppendUnique(result.missing_layers, "FVR-M-alignment-integration");
    }
    if (c.has_chas_alignment_layer) {
        AppendUnique(result.active_layers, "FVR-H-chas-alignment");
    } else {
        AppendUnique(result.missing_layers, "FVR-H-chas-alignment");
    }
    if (c.has_candidate_equivalence_layer) {
        AppendUnique(result.active_layers, "FVR-J-candidate-equivalence");
    } else {
        AppendUnique(result.missing_layers, "FVR-J-candidate-equivalence");
    }
    if (c.has_global_candidate_equivalence_layer) {
        AppendUnique(result.active_layers, "FVR-K-global-candidate-equivalence");
    } else {
        AppendUnique(result.missing_layers, "FVR-K-global-candidate-equivalence");
    }
    if (!c.no_fundamentally_blocked_draw) {
        AppendUnique(result.missing_layers, "DRAW-proof-gate");
    }
}

} // namespace

const char* EquivalenceClosureStatusToString(const EquivalenceClosureStatus status)
{
    switch (status) {
    case EquivalenceClosureStatus::Open:
        return "OPEN";
    case EquivalenceClosureStatus::PartiallyClosable:
        return "PARTIAL";
    case EquivalenceClosureStatus::Closable:
        return "READY";
    case EquivalenceClosureStatus::Prohibited:
        return "PROHIBITED";
    }
    return "OPEN";
}

bool ClosureAllowed(const EquivalenceClosureCondition& condition)
{
    return condition.has_runtime_witness_chain &&
           condition.has_invariant_pass &&
           condition.has_candidate_equivalence_layer &&
           condition.has_global_candidate_equivalence_layer &&
           condition.has_chas_alignment_layer &&
           condition.has_coq_catalog_alignment_layer &&
           condition.no_fundamentally_blocked_draw;
}

EquivalenceClosureResult BuildEquivalenceClosure(
    const std::optional<RuntimeSupplyWitness>& witness,
    const std::optional<RuntimeSupplyWitnessCoqCatalogRow>& catalog,
    const std::optional<RuntimeSupplyChasAlignmentReport>& chas,
    const std::optional<WitnessEquivalenceCandidate>& candidate,
    const std::optional<GlobalCandidateEquivalenceObservation>& global)
{
    EquivalenceClosureResult result;
    result.equivalence_claimed = false;

    if (!witness.has_value() || !catalog.has_value()) {
        result.status = EquivalenceClosureStatus::Open;
        result.closure_allowed = false;
        PopulateLayerLists(result);
        return result;
    }

    result.condition.has_runtime_witness_chain =
        RuntimeWitnessChainOk(*witness, *catalog);
    result.condition.has_invariant_pass =
        witness->invariants_passed && catalog->invariants_passed;
    result.condition.has_coq_catalog_alignment_layer = CoqCatalogAlignmentOk(*catalog);

    if (chas.has_value()) {
        result.condition.has_chas_alignment_layer = ChasAlignmentOk(*chas);
    }

    result.condition.has_candidate_equivalence_layer =
        kCandidateEquivalenceLayerEstablished && candidate.has_value();
    result.condition.has_global_candidate_equivalence_layer =
        kGlobalCandidateEquivalenceLayerEstablished && global.has_value();

    if (candidate.has_value()) {
        result.condition.no_fundamentally_blocked_draw =
            !HasReason(candidate->blocking_reasons, DRAW_BLOCKED_REASON);
    }

    if (chas.has_value() && candidate.has_value()) {
        result.status = ClassifyClosureStatus(result.condition, *candidate, *chas);
    } else {
        result.status = EquivalenceClosureStatus::Open;
    }

    result.closure_allowed = ClosureAllowed(result.condition);
    PopulateLayerLists(result);
    return result;
}

std::optional<EquivalenceClosureResult> ResolveEquivalenceClosureAt(const CBlockIndex* pindex)
{
    if (pindex == nullptr) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitness> witness =
        GetRuntimeSupplyWitness(pindex);
    const std::optional<RuntimeSupplyWitnessCoqCatalogRow> catalog =
        ResolveWitnessCoqCatalogRowAt(pindex);
    const std::optional<RuntimeSupplyChasAlignmentReport> chas = ResolveChasAlignmentAt(pindex);
    const std::optional<WitnessEquivalenceCandidate> candidate =
        ResolveWitnessEquivalenceCandidateAt(pindex);
    const std::optional<GlobalCandidateEquivalenceObservation> global =
        ResolveGlobalCandidateEquivalenceAt(pindex);

    if (!witness.has_value() && !catalog.has_value()) {
        return std::nullopt;
    }

    return BuildEquivalenceClosure(witness, catalog, chas, candidate, global);
}

UniValue EquivalenceClosureResultToJson(const EquivalenceClosureResult& result)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("task", "FVR-L");
    obj.pushKV("closure_status", EquivalenceClosureStatusToString(result.status));
    obj.pushKV("closure_allowed", result.closure_allowed);
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("pd_4f_ob_02", "OPEN");
    obj.pushKV("pd_4f_cpp_01", "OPEN");

    UniValue condition(UniValue::VOBJ);
    condition.pushKV("has_runtime_witness_chain", result.condition.has_runtime_witness_chain);
    condition.pushKV("has_invariant_pass", result.condition.has_invariant_pass);
    condition.pushKV("has_candidate_equivalence_layer",
                     result.condition.has_candidate_equivalence_layer);
    condition.pushKV("has_global_candidate_equivalence_layer",
                     result.condition.has_global_candidate_equivalence_layer);
    condition.pushKV("has_chas_alignment_layer", result.condition.has_chas_alignment_layer);
    condition.pushKV("has_coq_catalog_alignment_layer",
                     result.condition.has_coq_catalog_alignment_layer);
    condition.pushKV("no_fundamentally_blocked_draw",
                     result.condition.no_fundamentally_blocked_draw);
    obj.pushKV("EquivalenceClosureCondition", std::move(condition));

    UniValue missing(UniValue::VARR);
    for (const std::string& layer : result.missing_layers) {
        missing.push_back(layer);
    }
    obj.pushKV("missing_layers", std::move(missing));

    UniValue active(UniValue::VARR);
    for (const std::string& layer : result.active_layers) {
        active.push_back(layer);
    }
    obj.pushKV("active_layers", std::move(active));
    return obj;
}

} // namespace Consensus
