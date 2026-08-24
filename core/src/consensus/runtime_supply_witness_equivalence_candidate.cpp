// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_witness_equivalence_candidate.h>

#include <chain.h>
#include <core_io.h>
#include <univalue.h>

#include <utility>

namespace Consensus {

namespace {

constexpr const char* DRAW_BLOCKED_REASON = "DRAW-CES-FundamentallyBlocked";

bool CoqOruSupplyPresent(const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    return catalog.height >= 0;
}

bool CoqResSupplyPresent(const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    return catalog.height >= 0;
}

bool ParentDeltaPresent(const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    return catalog.delta.parent_witness_present;
}

CandidateEquivalenceStatus ClassifySupplyCandidate(
    bool runtime_witness_present,
    bool coq_field_present,
    bool parent_delta_present)
{
    if (!runtime_witness_present || !coq_field_present) {
        return CandidateEquivalenceStatus::NotComparable;
    }
    if (parent_delta_present) {
        return CandidateEquivalenceStatus::Comparable;
    }
    return CandidateEquivalenceStatus::PartiallyComparable;
}

CandidateEquivalenceStatus ClassifyInvariantCandidate(
    const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    if (!catalog.invariants_passed) {
        return CandidateEquivalenceStatus::FundamentallyBlocked;
    }
    if (catalog.height > 0 && catalog.delta.parent_witness_present &&
        !catalog.delta.witness_chain_step_valid) {
        return CandidateEquivalenceStatus::FundamentallyBlocked;
    }
    if (catalog.height > 0 && !catalog.delta.witness_chain_step_valid) {
        return CandidateEquivalenceStatus::FundamentallyBlocked;
    }
    return CandidateEquivalenceStatus::Comparable;
}

CandidateEquivalenceStatus ClassifyWitnessChainCandidate(
    bool runtime_witness_present,
    const RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    if (!runtime_witness_present) {
        return CandidateEquivalenceStatus::NotComparable;
    }
    if (catalog.height == 0) {
        return catalog.delta.witness_chain_step_valid
                   ? CandidateEquivalenceStatus::Comparable
                   : CandidateEquivalenceStatus::FundamentallyBlocked;
    }
    if (!catalog.delta.parent_witness_present) {
        return CandidateEquivalenceStatus::PartiallyComparable;
    }
    if (!catalog.delta.witness_chain_step_valid) {
        return CandidateEquivalenceStatus::FundamentallyBlocked;
    }
    return CandidateEquivalenceStatus::Comparable;
}

void AppendUnique(std::vector<std::string>& reasons, const std::string& reason)
{
    if (reason.empty()) {
        return;
    }
    for (const std::string& existing : reasons) {
        if (existing == reason) {
            return;
        }
    }
    reasons.push_back(reason);
}

void CollectSupplyBlockingReasons(std::vector<std::string>& reasons,
                                  const char* field_label,
                                  CandidateEquivalenceStatus status,
                                  bool runtime_witness_present,
                                  bool coq_field_present,
                                  bool parent_delta_present)
{
    if (status == CandidateEquivalenceStatus::Comparable) {
        return;
    }
    if (!runtime_witness_present) {
        AppendUnique(reasons, std::string(field_label) + "-RUNTIME-WITNESS-MISSING");
    }
    if (!coq_field_present) {
        AppendUnique(reasons, std::string(field_label) + "-COQ-CATALOG-MISSING");
    }
    if (runtime_witness_present && coq_field_present && !parent_delta_present) {
        AppendUnique(reasons, std::string(field_label) + "-PARENT-DELTA-MISSING");
    }
    if (status == CandidateEquivalenceStatus::NotComparable) {
        AppendUnique(reasons, std::string(field_label) + "-CES-NotComparable");
    }
}

void CollectChasBlockingReasons(std::vector<std::string>& reasons,
                                const RuntimeSupplyChasAlignmentReport& chas)
{
    if (chas.chas_oru_state.status == CanonicalHashAlignmentStatus::FundamentallyBlocked) {
        AppendUnique(reasons, "CHAS-ORU-FundamentallyBlocked");
    }
    if (chas.chas_res_state.status == CanonicalHashAlignmentStatus::FundamentallyBlocked) {
        AppendUnique(reasons, "CHAS-RES-FundamentallyBlocked");
    }
    for (const std::string& id : chas.delta_audit_violations) {
        AppendUnique(reasons, id);
    }
    for (const std::string& id : chas.violations) {
        AppendUnique(reasons, id);
    }
}

} // namespace

const char* CandidateEquivalenceStatusToString(const CandidateEquivalenceStatus status)
{
    switch (status) {
    case CandidateEquivalenceStatus::Comparable:
        return "CES_Comparable";
    case CandidateEquivalenceStatus::PartiallyComparable:
        return "CES_PartiallyComparable";
    case CandidateEquivalenceStatus::NotComparable:
        return "CES_NotComparable";
    case CandidateEquivalenceStatus::FundamentallyBlocked:
        return "CES_FundamentallyBlocked";
    }
    return "CES_NotComparable";
}

WitnessEquivalenceCandidate BuildWitnessEquivalenceCandidate(
    const RuntimeSupplyWitness& witness,
    const RuntimeSupplyWitnessCoqCatalogRow& catalog,
    const RuntimeSupplyChasAlignmentReport& chas)
{
    WitnessEquivalenceCandidate candidate;
    candidate.height = witness.height;
    candidate.block_hash = witness.block_hash;
    candidate.equivalence_claimed = false;

    const bool runtime_witness_present = witness.height >= 0;
    const bool oru_coq_present = CoqOruSupplyPresent(catalog);
    const bool res_coq_present = CoqResSupplyPresent(catalog);
    const bool parent_delta_present = ParentDeltaPresent(catalog);

    candidate.oru_supply_status = ClassifySupplyCandidate(
        runtime_witness_present, oru_coq_present, parent_delta_present);
    candidate.res_supply_status = ClassifySupplyCandidate(
        runtime_witness_present, res_coq_present, parent_delta_present);
    candidate.invariant_status = ClassifyInvariantCandidate(catalog);
    candidate.witness_chain_status =
        ClassifyWitnessChainCandidate(runtime_witness_present, catalog);

    CollectSupplyBlockingReasons(candidate.blocking_reasons,
                                 "ORU",
                                 candidate.oru_supply_status,
                                 runtime_witness_present,
                                 oru_coq_present,
                                 parent_delta_present);
    CollectSupplyBlockingReasons(candidate.blocking_reasons,
                                 "RES",
                                 candidate.res_supply_status,
                                 runtime_witness_present,
                                 res_coq_present,
                                 parent_delta_present);

    if (candidate.invariant_status == CandidateEquivalenceStatus::FundamentallyBlocked) {
        if (!catalog.invariants_passed) {
            AppendUnique(candidate.blocking_reasons, "INVARIANT-CES-FundamentallyBlocked");
            for (const std::string& id : catalog.violations) {
                AppendUnique(candidate.blocking_reasons, id);
            }
        } else {
            AppendUnique(candidate.blocking_reasons, "WITNESS-CHAIN-INVARIANT-BLOCKED");
        }
    }

    if (candidate.witness_chain_status == CandidateEquivalenceStatus::FundamentallyBlocked) {
        AppendUnique(candidate.blocking_reasons, "WITNESS-CHAIN-CES-FundamentallyBlocked");
    } else if (candidate.witness_chain_status == CandidateEquivalenceStatus::PartiallyComparable) {
        AppendUnique(candidate.blocking_reasons, "WITNESS-CHAIN-PARENT-MISSING");
    }

    AppendUnique(candidate.blocking_reasons, DRAW_BLOCKED_REASON);
    CollectChasBlockingReasons(candidate.blocking_reasons, chas);

    AppendUnique(candidate.blocking_reasons, "PD-4F-OB-02-OPEN");
    AppendUnique(candidate.blocking_reasons, "PD-4F-CPP-01-OPEN");

    return candidate;
}

std::optional<WitnessEquivalenceCandidate> ResolveWitnessEquivalenceCandidateAt(const CBlockIndex* pindex)
{
    if (pindex == nullptr) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitness> witness = GetRuntimeSupplyWitness(pindex);
    if (!witness.has_value()) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitnessCoqCatalogRow> catalog =
        ResolveWitnessCoqCatalogRowAt(pindex);
    if (!catalog.has_value()) {
        return std::nullopt;
    }

    const RuntimeSupplyChasAlignmentReport chas = BuildChasAlignmentFromCatalogRow(*catalog);
    return BuildWitnessEquivalenceCandidate(*witness, *catalog, chas);
}

UniValue WitnessEquivalenceCandidateToJson(const WitnessEquivalenceCandidate& candidate)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", candidate.height);
    obj.pushKV("block_hash", candidate.block_hash.GetHex());
    obj.pushKV("oru_supply_status",
               CandidateEquivalenceStatusToString(candidate.oru_supply_status));
    obj.pushKV("res_supply_status",
               CandidateEquivalenceStatusToString(candidate.res_supply_status));
    obj.pushKV("invariant_status",
               CandidateEquivalenceStatusToString(candidate.invariant_status));
    obj.pushKV("witness_chain_status",
               CandidateEquivalenceStatusToString(candidate.witness_chain_status));
    UniValue reasons(UniValue::VARR);
    for (const std::string& reason : candidate.blocking_reasons) {
        reasons.push_back(reason);
    }
    obj.pushKV("blocking_reasons", std::move(reasons));
    obj.pushKV("equivalence_claimed", candidate.equivalence_claimed);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    return obj;
}

std::optional<CoqEquivalenceCandidateExport> ResolveCoqEquivalenceCandidateExportAt(const CBlockIndex* pindex)
{
    if (pindex == nullptr) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitness> witness = GetRuntimeSupplyWitness(pindex);
    if (!witness.has_value()) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitnessCoqCatalogRow> catalog =
        ResolveWitnessCoqCatalogRowAt(pindex);
    if (!catalog.has_value()) {
        return std::nullopt;
    }

    CoqEquivalenceCandidateExport exp;
    exp.witness = *witness;
    exp.catalog = *catalog;
    exp.chas = BuildChasAlignmentFromCatalogRow(*catalog);
    exp.candidate = BuildWitnessEquivalenceCandidate(exp.witness, exp.catalog, exp.chas);
    return exp;
}

UniValue CoqEquivalenceCandidateExportToJson(const CoqEquivalenceCandidateExport& exp)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("task", "FVR-J");
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("pd_4f_ob_02", "OPEN");
    obj.pushKV("pd_4f_cpp_01", "OPEN");
    obj.pushKV("runtime_candidate", WitnessEquivalenceCandidateToJson(exp.candidate));

    UniValue coq_layer(UniValue::VOBJ);

    UniValue rw(UniValue::VOBJ);
    rw.pushKV("rw_height", static_cast<int>(exp.witness.height));
    rw.pushKV("rw_block_hash", exp.witness.block_hash.GetHex());
    rw.pushKV("rw_oru_supply_cumulative", ValueFromAmount(exp.witness.oru_supply_cumulative));
    rw.pushKV("rw_res_mint_cumulative", ValueFromAmount(exp.witness.res_mint_cumulative));
    rw.pushKV("rw_invariants_passed", exp.witness.invariants_passed);
    coq_layer.pushKV("RuntimeWitness", std::move(rw));

    UniValue row(UniValue::VOBJ);
    row.pushKV("cwr_height", static_cast<int>(exp.catalog.height));
    row.pushKV("cwr_block_hash", exp.catalog.block_hash.GetHex());
    row.pushKV("cwr_rs_oru_supply", ValueFromAmount(exp.catalog.rs_oru_supply));
    row.pushKV("cwr_rs_res_supply", ValueFromAmount(exp.catalog.rs_res_supply));
    row.pushKV("cwr_invariants_passed", exp.catalog.invariants_passed);
    row.pushKV("cwr_parent_witness_present", exp.catalog.delta.parent_witness_present);
    row.pushKV("cwr_witness_chain_step_valid", exp.catalog.delta.witness_chain_step_valid);
    coq_layer.pushKV("CoqWitnessRow", std::move(row));

    UniValue chas(UniValue::VOBJ);
    chas.pushKV("chas_oru_fundamentally_blocked",
                exp.chas.chas_oru_state.status == CanonicalHashAlignmentStatus::FundamentallyBlocked);
    chas.pushKV("chas_res_fundamentally_blocked",
                exp.chas.chas_res_state.status == CanonicalHashAlignmentStatus::FundamentallyBlocked);
    UniValue delta_violations(UniValue::VARR);
    for (const std::string& id : exp.chas.delta_audit_violations) {
        delta_violations.push_back(id);
    }
    chas.pushKV("chas_delta_violations", std::move(delta_violations));
    coq_layer.pushKV("CHASReport", std::move(chas));

    UniValue obs(UniValue::VOBJ);
    obs.pushKV("ceo_height", static_cast<int>(exp.candidate.height));
    obs.pushKV("ceo_block_hash", exp.candidate.block_hash.GetHex());
    obs.pushKV("ceo_oru_supply_status",
               CandidateEquivalenceStatusToString(exp.candidate.oru_supply_status));
    obs.pushKV("ceo_res_supply_status",
               CandidateEquivalenceStatusToString(exp.candidate.res_supply_status));
    obs.pushKV("ceo_invariant_status",
               CandidateEquivalenceStatusToString(exp.candidate.invariant_status));
    obs.pushKV("ceo_witness_chain_status",
               CandidateEquivalenceStatusToString(exp.candidate.witness_chain_status));
    coq_layer.pushKV("CandidateEquivalenceObservation", std::move(obs));

    obj.pushKV("coq_formal_layer", std::move(coq_layer));
    return obj;
}

} // namespace Consensus
