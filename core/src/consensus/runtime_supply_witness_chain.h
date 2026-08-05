// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_WITNESS_CHAIN_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_WITNESS_CHAIN_H

#include <consensus/amount.h>
#include <consensus/runtime_supply_invariant_checker.h>
#include <consensus/runtime_supply_state.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Consensus {

/**
 * TASK-FVR-F: per-block runtime formal witness (observation only).
 *
 * Captures invariant verification outcome at ConnectBlock time. Memory-only;
 * not serialized; must not affect consensus, validation, state_hash, or replay.
 */
struct RuntimeSupplyWitness {
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;
    bool invariants_passed{true};
    uint32_t checked_invariants{RUNTIME_SUPPLY_INVARIANT_COUNT};
    std::vector<std::string> violations;
    CAmount oru_supply_cumulative{0};
    CAmount res_mint_cumulative{0};
};

/** Build witness from captured mirror + invariant result (ConnectBlock path). */
RuntimeSupplyWitness BuildRuntimeSupplyWitness(const RuntimeSupplyState& state,
                                               const RuntimeInvariantResult& invariants);

/** Store witness in memory cache keyed by height. */
void StoreRuntimeSupplyWitness(RuntimeSupplyWitness witness);

/** @return Witness at `height` if stored. */
std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(int32_t height);

/** @return Witness at highest stored height, if any. */
std::optional<RuntimeSupplyWitness> GetLatestRuntimeSupplyWitness();

/** Drop witness on disconnect / reorg (paired with mirror invalidation). */
void InvalidateRuntimeSupplyWitness(int32_t height);

/**
 * Verify parent→child witness linkage (WITNESS-CHAIN-1 / WITNESS-CHAIN-2).
 *
 * child.height = parent.height + 1
 * child.parent_hash = parent.block_hash
 */
bool VerifyRuntimeWitnessChainStep(const RuntimeSupplyWitness& parent,
                                   const RuntimeSupplyWitness& child);

/** Clear in-memory witness cache (unit tests only). */
void ClearRuntimeSupplyWitnessCacheForTest();

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_WITNESS_CHAIN_H
