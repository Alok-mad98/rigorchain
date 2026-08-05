// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_INVARIANT_CHECKER_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_INVARIANT_CHECKER_H

#include <consensus/runtime_supply_state.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Consensus {
struct Params;
} // namespace Consensus

namespace Consensus {

/** Number of formal supply invariants verified (ORU-L1..L4 + RES-L1..L6). */
inline constexpr uint32_t RUNTIME_SUPPLY_INVARIANT_COUNT = 10;

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
 * Verify ORU-L1..L4 and RES-L1..L6 on a captured mirror.
 *
 * ORU-L4 requires `consensus` to evaluate `GetBlockSubsidy(height)`.
 * Violation labels are stable catalog ids (e.g. `"ORU-L3"`, `"RES-L5"`).
 */
RuntimeInvariantResult VerifyRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                     const Params& consensus);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_INVARIANT_CHECKER_H
