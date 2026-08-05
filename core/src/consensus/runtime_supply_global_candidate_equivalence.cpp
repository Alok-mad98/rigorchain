// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_global_candidate_equivalence.h>

#include <chain.h>
#include <univalue.h>

#include <utility>

namespace Consensus {

namespace {

CandidateEquivalenceStatus ChasFieldToCandidateStatus(const CanonicalHashAlignmentStatus chas)
{
    switch (chas) {
    case CanonicalHashAlignmentStatus::FundamentallyBlocked:
        return CandidateEquivalenceStatus::FundamentallyBlocked;
    case CanonicalHashAlignmentStatus::Missing:
        return CandidateEquivalenceStatus::NotComparable;
    case CanonicalHashAlignmentStatus::Partial:
        return CandidateEquivalenceStatus::PartiallyComparable;
    case CanonicalHashAlignmentStatus::Aligned:
        return CandidateEquivalenceStatus::Comparable;
    }
    return CandidateEquivalenceStatus::NotComparable;
}

CandidateEquivalenceStatus MergeCandidateStatus(const CandidateEquivalenceStatus a,
                                                const CandidateEquivalenceStatus b)
{
    auto severity = [](const CandidateEquivalenceStatus s) {
        switch (s) {
        case CandidateEquivalenceStatus::FundamentallyBlocked:
            return 3;
        case CandidateEquivalenceStatus::NotComparable:
            return 2;
        case CandidateEquivalenceStatus::PartiallyComparable:
            return 1;
        case CandidateEquivalenceStatus::Comparable:
            return 0;
        }
        return 0;
    };
    return severity(a) >= severity(b) ? a : b;
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

} // namespace

GlobalCandidateEquivalenceObservation BuildGlobalCandidateEquivalence(
    const RuntimeSupplyChasAlignmentReport& chas,
    const WitnessEquivalenceCandidate& candidate)
{
    GlobalCandidateEquivalenceObservation global;
    global.height = candidate.height;
    global.block_hash = candidate.block_hash;
    global.equivalence_claimed = false;

    const CandidateEquivalenceStatus chas_oru =
        ChasFieldToCandidateStatus(chas.chas_oru_state.status);
    const CandidateEquivalenceStatus chas_res =
        ChasFieldToCandidateStatus(chas.chas_res_state.status);

    const CandidateEquivalenceStatus shared_context = MergeCandidateStatus(
        candidate.invariant_status, candidate.witness_chain_status);

    global.oru_global_status = MergeCandidateStatus(
        chas_oru, MergeCandidateStatus(candidate.oru_supply_status, shared_context));
    global.res_global_status = MergeCandidateStatus(
        chas_res, MergeCandidateStatus(candidate.res_supply_status, shared_context));

    global.blocking_reasons = candidate.blocking_reasons;
    for (const std::string& id : chas.delta_audit_violations) {
        AppendUnique(global.blocking_reasons, id);
    }
    for (const std::string& id : chas.violations) {
        AppendUnique(global.blocking_reasons, id);
    }
    AppendUnique(global.blocking_reasons, "GLOBAL-CHAS-CANDIDATE-AGGREGATE");
    AppendUnique(global.blocking_reasons, "PD-4F-OB-02-OPEN");
    AppendUnique(global.blocking_reasons, "PD-4F-CPP-01-OPEN");

    return global;
}

std::optional<GlobalCandidateEquivalenceObservation> ResolveGlobalCandidateEquivalenceAt(
    const CBlockIndex* pindex)
{
    const std::optional<WitnessEquivalenceCandidate> candidate =
        ResolveWitnessEquivalenceCandidateAt(pindex);
    if (!candidate.has_value()) {
        return std::nullopt;
    }
    const std::optional<RuntimeSupplyChasAlignmentReport> chas = ResolveChasAlignmentAt(pindex);
    if (!chas.has_value()) {
        return std::nullopt;
    }
    return BuildGlobalCandidateEquivalence(*chas, *candidate);
}

std::optional<GlobalCandidateEquivalenceExport> ResolveGlobalCandidateEquivalenceExportAt(
    const CBlockIndex* pindex)
{
    const std::optional<WitnessEquivalenceCandidate> candidate =
        ResolveWitnessEquivalenceCandidateAt(pindex);
    if (!candidate.has_value()) {
        return std::nullopt;
    }
    const std::optional<RuntimeSupplyChasAlignmentReport> chas = ResolveChasAlignmentAt(pindex);
    if (!chas.has_value()) {
        return std::nullopt;
    }
    GlobalCandidateEquivalenceExport exp;
    exp.candidate = *candidate;
    exp.chas = *chas;
    exp.global = BuildGlobalCandidateEquivalence(exp.chas, exp.candidate);
    return exp;
}

UniValue GlobalCandidateEquivalenceExportToJson(const GlobalCandidateEquivalenceExport& exp)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("task", "FVR-K");
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("pd_4f_ob_02", "OPEN");
    obj.pushKV("pd_4f_cpp_01", "OPEN");

    UniValue global(UniValue::VOBJ);
    global.pushKV("gceo_height", exp.global.height);
    global.pushKV("gceo_block_hash", exp.global.block_hash.GetHex());
    global.pushKV("gceo_oru_global_status",
                  CandidateEquivalenceStatusToString(exp.global.oru_global_status));
    global.pushKV("gceo_res_global_status",
                  CandidateEquivalenceStatusToString(exp.global.res_global_status));
    UniValue reasons(UniValue::VARR);
    for (const std::string& reason : exp.global.blocking_reasons) {
        reasons.push_back(reason);
    }
    global.pushKV("gceo_blocking_reasons", std::move(reasons));
    obj.pushKV("GlobalCandidateEquivalenceObservation", std::move(global));

    obj.pushKV("chas_report", ChasAlignmentReportToJson(exp.chas));
    obj.pushKV("runtime_candidate", WitnessEquivalenceCandidateToJson(exp.candidate));
    return obj;
}

} // namespace Consensus
