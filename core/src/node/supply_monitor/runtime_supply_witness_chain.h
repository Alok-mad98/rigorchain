// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_CHAIN_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_CHAIN_H

#include <consensus/amount.h>
#include <node/supply_monitor/runtime_supply_invariant_checker.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;

namespace Consensus {

/**
 * / (W7): per-block runtime formal
 * witness (observation only).
 *
 * Captures invariant verification outcome at ConnectBlock time. Memory-only;
 * not serialized; must not affect consensus, validation, state_hash, or replay.
 *
 * FENCE-W7: keyed by **block hash**, never by height. Two forks at the same
 * height must not overwrite each other. Do not read this cache in a ConnectBlock
 * accept/reject predicate. Height-only lookup is intentionally omitted.
 */
struct RuntimeSupplyWitness {
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;
    bool invariants_passed{true};
    uint32_t checked_invariants{RUNTIME_SUPPLY_INVARIANT_COUNT};
    std::vector<std::string> violations;
    CAmount oru_supply_cumulative{0}; //!< RFC-002 ledger S_ORU; not spendable UTXO
    CAmount res_mint_cumulative{0};   //!< S_RES ledger; not spendable UTXO
};

/** Build witness from captured mirror + invariant result (ConnectBlock path). */
RuntimeSupplyWitness BuildRuntimeSupplyWitness(const RuntimeSupplyState& state,
                                               const RuntimeInvariantResult& invariants);

/**
 * Store witness in the memory cache keyed by `witness.block_hash`.
 * Null block_hash is refused (no degenerate key).
 */
void StoreRuntimeSupplyWitness(RuntimeSupplyWitness witness);

/** @return Witness for `block_hash` if stored. Never a height index. */
std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(const uint256& block_hash);

/**
 * Observational lookup for a block index. Uses `GetBlockHash()`, never height.
 * @return nullopt if `pindex` is null or has no hash pointer.
 */
std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(const CBlockIndex* pindex);

/** @return Witness at the highest stored height (last writer wins ties). Observational. */
std::optional<RuntimeSupplyWitness> GetLatestRuntimeSupplyWitness();

/** Drop witness on disconnect / reorg (keyed by block hash). */
void InvalidateRuntimeSupplyWitness(const uint256& block_hash);

/** Drop witness for `pindex->GetBlockHash()` if the index is hashed. */
void InvalidateRuntimeSupplyWitness(const CBlockIndex* pindex);

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

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_CHAIN_H
