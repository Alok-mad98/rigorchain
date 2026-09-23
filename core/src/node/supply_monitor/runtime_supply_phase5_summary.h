// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_PHASE5_SUMMARY_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_PHASE5_SUMMARY_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/** TASK-FVR-N: canonical Phase-5 layer summary (FVR-A..M snapshot). */
enum class Phase5LayerStatus {
    Established,
    Active,
    Partial,
    Blocked,
    Impossible,
};

struct Phase5LayerSummary {
    std::string task_id;
    std::string layer_name;
    Phase5LayerStatus status{Phase5LayerStatus::Active};
    bool layer_established{true};
    bool coq_eq_runtime{false};
    bool runtime_present{false};
};

struct Phase5SummaryExport {
    int32_t height{-1};
    std::vector<Phase5LayerSummary> layers;
    std::string closure_status{"PROHIBITED"};
    bool closure_allowed{false};
    std::string freeze_protocol_link;
    std::string freeze_note;
    bool equivalence_claimed{false};
};

const char* Phase5LayerStatusToString(Phase5LayerStatus status);

std::optional<Phase5SummaryExport> ResolvePhase5SummaryAt(const CBlockIndex* pindex);

UniValue Phase5SummaryExportToJson(const Phase5SummaryExport& exp);

struct Phase5SummaryFinalExport {
    Phase5SummaryExport base;
    std::string pd13_status{"CLOSED"};
    std::string pd14_status{"CLOSED"};
    std::string pd15_status{"CLOSED"};
    std::string pd4f_ob02_status{"READY"};
    std::string pd4f_cpp01_status{"OPEN"};
    bool cpp01_runtime_alignment_ready{false};
    bool cpp01_vector_alignment_ready{false};
    bool cpp01_coq_alignment_ready{false};
    bool cpp01_candidate_equivalence_ready{false};
    bool candidate_equivalence_evidence_ready{false};
    bool candidate_equivalence_runtime_ready{false};
    int golden_vector_total{12};
    int golden_vector_materialized{9};
    int ecs_open_count{3};
    int ecs_partial_count{7};
    int ecs_prohibited_count{2};
};

std::optional<Phase5SummaryFinalExport> ResolvePhase5SummaryFinalAt(const CBlockIndex* pindex);

UniValue Phase5SummaryFinalExportToJson(const Phase5SummaryFinalExport& exp);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_PHASE5_SUMMARY_H
