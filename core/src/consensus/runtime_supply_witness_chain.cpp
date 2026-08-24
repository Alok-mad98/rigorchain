// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_witness_chain.h>

#include <chain.h>

#include <map>
#include <utility>

namespace Consensus {
namespace {

/**
 * FENCE-W7: observational cache keyed by block hash.
 * Not a consensus input. Not a spendable balance. Not indexed by height
 * (height-only keys would overwrite sibling forks).
 */
std::map<uint256, RuntimeSupplyWitness> g_witness_by_block_hash;
uint256 g_latest_witness_hash{};

void RecomputeLatestWitnessHash()
{
    g_latest_witness_hash.SetNull();
    int32_t best_height = -1;
    for (const auto& [hash, witness] : g_witness_by_block_hash) {
        if (witness.height > best_height) {
            best_height = witness.height;
            g_latest_witness_hash = hash;
        } else if (witness.height == best_height) {
            // Last writer at the max height wins the "latest" pointer only.
            g_latest_witness_hash = hash;
        }
    }
}

} // namespace

RuntimeSupplyWitness BuildRuntimeSupplyWitness(const RuntimeSupplyState& state,
                                               const RuntimeInvariantResult& invariants)
{
    RuntimeSupplyWitness witness;
    witness.height = state.height;
    witness.block_hash = state.block_hash;
    witness.parent_hash = state.parent_hash;
    witness.invariants_passed = invariants.passed;
    witness.checked_invariants = invariants.checked_invariants;
    witness.violations = invariants.violations;
    witness.oru_supply_cumulative = state.oru_supply_cumulative;
    witness.res_mint_cumulative = state.res_mint_cumulative;
    return witness;
}

void StoreRuntimeSupplyWitness(RuntimeSupplyWitness witness)
{
    if (witness.block_hash.IsNull()) {
        return;
    }
    const int32_t height = witness.height;
    const uint256 hash = witness.block_hash;
    g_witness_by_block_hash[hash] = std::move(witness);

    if (g_latest_witness_hash.IsNull()) {
        g_latest_witness_hash = hash;
        return;
    }
    const auto it = g_witness_by_block_hash.find(g_latest_witness_hash);
    if (it == g_witness_by_block_hash.end() || height >= it->second.height) {
        g_latest_witness_hash = hash;
    }
}

std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(const uint256& block_hash)
{
    if (block_hash.IsNull()) {
        return std::nullopt;
    }
    const auto it = g_witness_by_block_hash.find(block_hash);
    if (it == g_witness_by_block_hash.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(const CBlockIndex* pindex)
{
    if (pindex == nullptr || pindex->phashBlock == nullptr) {
        return std::nullopt;
    }
    return GetRuntimeSupplyWitness(pindex->GetBlockHash());
}

std::optional<RuntimeSupplyWitness> GetLatestRuntimeSupplyWitness()
{
    if (g_latest_witness_hash.IsNull()) {
        return std::nullopt;
    }
    return GetRuntimeSupplyWitness(g_latest_witness_hash);
}

void InvalidateRuntimeSupplyWitness(const uint256& block_hash)
{
    if (block_hash.IsNull()) {
        return;
    }
    g_witness_by_block_hash.erase(block_hash);
    if (g_latest_witness_hash == block_hash) {
        RecomputeLatestWitnessHash();
    }
}

void InvalidateRuntimeSupplyWitness(const CBlockIndex* pindex)
{
    if (pindex == nullptr || pindex->phashBlock == nullptr) {
        return;
    }
    InvalidateRuntimeSupplyWitness(pindex->GetBlockHash());
}

bool VerifyRuntimeWitnessChainStep(const RuntimeSupplyWitness& parent,
                                   const RuntimeSupplyWitness& child)
{
    if (child.height != parent.height + 1) {
        return false;
    }
    if (child.parent_hash != parent.block_hash) {
        return false;
    }
    return true;
}

void ClearRuntimeSupplyWitnessCacheForTest()
{
    g_witness_by_block_hash.clear();
    g_latest_witness_hash.SetNull();
}

} // namespace Consensus
