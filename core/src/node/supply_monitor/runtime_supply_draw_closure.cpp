// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_draw_closure.h>

#include <univalue.h>

namespace Consensus {

namespace {

DrawClosureStatus ClassifyDrawClosureStatus(const DrawClosureCondition& condition)
{
    if (!condition.has_draw_canonical_encoder || !condition.has_runtime_hash_kernel) {
        return DrawClosureStatus::Impossible;
    }
    if (!condition.has_runtime_trace || !condition.has_runtime_transition_history ||
        !condition.has_runtime_accept_history) {
        return DrawClosureStatus::NotClosable;
    }
    return DrawClosureStatus::PotentiallyClosable;
}

} // namespace

const char* DrawClosureStatusToString(const DrawClosureStatus status)
{
    switch (status) {
    case DrawClosureStatus::PotentiallyClosable:
        return "POTENTIALLY_CLOSABLE";
    case DrawClosureStatus::NotClosable:
        return "NOT_CLOSABLE";
    case DrawClosureStatus::Impossible:
        return "IMPOSSIBLE";
    }
    return "IMPOSSIBLE";
}

DrawClosureResult ResolveDrawClosureStatus()
{
    DrawClosureResult result;
    result.equivalence_claimed = false;

    // Frozen 018GG-I/J/K/L facts for current RIGOR Draw architecture.
    result.condition.has_runtime_trace = false;
    result.condition.has_runtime_transition_history = false;
    result.condition.has_runtime_accept_history = false;
    result.condition.has_runtime_hash_kernel = false;
    result.condition.has_draw_canonical_encoder = false;

    result.status = ClassifyDrawClosureStatus(result.condition);

    if (!result.condition.has_draw_canonical_encoder) {
        result.missing_requirements.emplace_back("DRAW-CANONICAL-ENCODER-MISSING");
    }
    if (!result.condition.has_runtime_hash_kernel) {
        result.missing_requirements.emplace_back("RUNTIME-HASH-KERNEL-MISSING");
    }
    if (!result.condition.has_runtime_trace) {
        result.missing_requirements.emplace_back("RUNTIME-TRACE-HISTORY-MISSING");
    }
    if (!result.condition.has_runtime_transition_history) {
        result.missing_requirements.emplace_back("RUNTIME-TRANSITION-HISTORY-MISSING");
    }
    if (!result.condition.has_runtime_accept_history) {
        result.missing_requirements.emplace_back("RUNTIME-ACCEPT-HISTORY-MISSING");
    }

    result.impossibility_reasons = {
        "DRAW-CES-FundamentallyBlocked",
        "018GG-K-CAS-FundamentallyBlocked",
        "018GG-I-GCS-NonClosable",
        "018GG-L-IFS-Missing",
        "018GG-H-RCS-NotReconstructable",
        "FVR-L-DRAW-PROOF-GATE-CLOSED",
    };

    return result;
}

UniValue DrawClosureResultToJson(const DrawClosureResult& result)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("task", "FVR-M");
    obj.pushKV("draw_status", DrawClosureStatusToString(result.status));
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("pd_4f_ob_02", "OPEN");
    obj.pushKV("pd_4f_cpp_01", "OPEN");

    UniValue condition(UniValue::VOBJ);
    condition.pushKV("has_runtime_trace", result.condition.has_runtime_trace);
    condition.pushKV("has_runtime_transition_history",
                     result.condition.has_runtime_transition_history);
    condition.pushKV("has_runtime_accept_history", result.condition.has_runtime_accept_history);
    condition.pushKV("has_runtime_hash_kernel", result.condition.has_runtime_hash_kernel);
    condition.pushKV("has_draw_canonical_encoder", result.condition.has_draw_canonical_encoder);
    obj.pushKV("DrawClosureCondition", std::move(condition));

    UniValue missing(UniValue::VARR);
    for (const std::string& req : result.missing_requirements) {
        missing.push_back(req);
    }
    obj.pushKV("missing_requirements", std::move(missing));

    UniValue reasons(UniValue::VARR);
    for (const std::string& reason : result.impossibility_reasons) {
        reasons.push_back(reason);
    }
    obj.pushKV("impossibility_reasons", std::move(reasons));
    return obj;
}

} // namespace Consensus
