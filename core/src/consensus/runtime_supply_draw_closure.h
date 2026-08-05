// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_DRAW_CLOSURE_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_DRAW_CLOSURE_H

#include <cstdint>
#include <string>
#include <vector>

class UniValue;

namespace Consensus {

/** TASK-FVR-M: Draw closure impossibility (architecture-level; no equivalence). */
enum class DrawClosureStatus {
    PotentiallyClosable,
    NotClosable,
    Impossible,
};

struct DrawClosureCondition {
    bool has_runtime_trace{false};
    bool has_runtime_transition_history{false};
    bool has_runtime_accept_history{false};
    bool has_runtime_hash_kernel{false};
    bool has_draw_canonical_encoder{false};
};

struct DrawClosureResult {
    DrawClosureStatus status{DrawClosureStatus::Impossible};
    DrawClosureCondition condition;
    std::vector<std::string> missing_requirements;
    std::vector<std::string> impossibility_reasons;
    bool equivalence_claimed{false};
};

const char* DrawClosureStatusToString(DrawClosureStatus status);

DrawClosureResult ResolveDrawClosureStatus();

UniValue DrawClosureResultToJson(const DrawClosureResult& result);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_DRAW_CLOSURE_H
