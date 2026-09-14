// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_INVARIANT_CHECKER_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_INVARIANT_CHECKER_H

#include <node/supply_monitor/runtime_supply_state.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Consensus {
struct Params;
} // namespace Consensus

namespace Consensus {

/**
 * Number of formal supply invariants verified (ORU-L1..L4 + RES-L1, L2, L4, L5, L6).
 *
 * RES-L3 retired with RFC-019: it pinned res_mint_block to the W(h) schedule, and there
 * is no schedule any more. RES-L6 survived in rewritten form; see the checker.
 */
inline constexpr uint32_t RUNTIME_SUPPLY_INVARIANT_COUNT = 9;

/**
 * TASK-FVR-E: runtime-executable formal invariant verification (observation only).
 *
 * Verifies that `RuntimeSupplyState` satisfies the same recurrence and bound
 * laws as the formal model. Does not affect consensus, validation, or state_hash.
 */
struct RuntimeInvariantResult {
    bool passed{true};
    std::vector<std::string> violations;
    uint32_t checked_invariants{RUNTIME_SUPPLY_INVARIANT_COUNT};
};

/**
 * Verify ORU-L1..L4 and RES-L1, L2, L4, L5 on a captured mirror.
 *
 * ORU-L4 requires `consensus` to evaluate `GetBlockSubsidy(height)`.
 * Violation labels are stable catalog ids (e.g. `"ORU-L3"`, `"RES-L5"`).
 */
RuntimeInvariantResult VerifyRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                     const Params& consensus);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_INVARIANT_CHECKER_H
