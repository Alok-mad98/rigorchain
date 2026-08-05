// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_witness_chain.h>

#include <map>
#include <utility>

namespace Consensus {
namespace {

std::map<int32_t, RuntimeSupplyWitness> g_witness_by_height;
int32_t g_latest_witness_height{-1};

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
    const int32_t height = witness.height;
    g_witness_by_height[height] = std::move(witness);
    if (height > g_latest_witness_height) {
        g_latest_witness_height = height;
    }
}

std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(int32_t height)
{
    const auto it = g_witness_by_height.find(height);
    if (it == g_witness_by_height.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<RuntimeSupplyWitness> GetLatestRuntimeSupplyWitness()
{
    if (g_latest_witness_height < 0) {
        return std::nullopt;
    }
    return GetRuntimeSupplyWitness(g_latest_witness_height);
}

void InvalidateRuntimeSupplyWitness(int32_t height)
{
    g_witness_by_height.erase(height);
    if (g_latest_witness_height == height) {
        g_latest_witness_height = g_witness_by_height.empty() ? -1 : g_witness_by_height.rbegin()->first;
    }
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
    g_witness_by_height.clear();
    g_latest_witness_height = -1;
}

} // namespace Consensus
