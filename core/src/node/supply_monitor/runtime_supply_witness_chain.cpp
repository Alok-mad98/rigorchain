// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_witness_chain.h>

#include <chain.h>
#include <sync.h>

#include <deque>
#include <map>
#include <utility>

namespace Consensus {
namespace {

/**
 * FENCE-W7: bounded observational cache keyed by block hash.
 * Not a consensus input. Not a spendable balance. Not indexed by height
 * (height-only keys would overwrite sibling forks).
 *
 * LRU-capped so ConnectBlock cannot grow process memory without bound (RIG-08).
 */
class WitnessCache
{
public:
    void Store(RuntimeSupplyWitness witness)
    {
        LOCK(m_mutex);
        if (witness.block_hash.IsNull()) {
            return;
        }
        const int32_t height = witness.height;
        const uint256 hash = witness.block_hash;
        if (m_entries.find(hash) == m_entries.end()) {
            m_order.push_back(hash);
        }
        m_entries[hash] = std::move(witness);

        if (m_latest_hash.IsNull()) {
            m_latest_hash = hash;
        } else {
            const auto it = m_entries.find(m_latest_hash);
            if (it == m_entries.end() || height >= it->second.height) {
                m_latest_hash = hash;
            }
        }

        while (m_order.size() > MAX_ENTRIES) {
            const uint256 evicted = m_order.front();
            m_order.pop_front();
            m_entries.erase(evicted);
            if (m_latest_hash == evicted) {
                RecomputeLatestLocked();
            }
        }
    }

    std::optional<RuntimeSupplyWitness> Get(const uint256& block_hash) const
    {
        LOCK(m_mutex);
        if (block_hash.IsNull()) {
            return std::nullopt;
        }
        const auto it = m_entries.find(block_hash);
        if (it == m_entries.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<RuntimeSupplyWitness> GetLatest() const
    {
        LOCK(m_mutex);
        if (m_latest_hash.IsNull()) {
            return std::nullopt;
        }
        const auto it = m_entries.find(m_latest_hash);
        if (it == m_entries.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void Invalidate(const uint256& block_hash)
    {
        LOCK(m_mutex);
        if (block_hash.IsNull()) {
            return;
        }
        m_entries.erase(block_hash);
        for (auto it = m_order.begin(); it != m_order.end(); ++it) {
            if (*it == block_hash) {
                m_order.erase(it);
                break;
            }
        }
        if (m_latest_hash == block_hash) {
            RecomputeLatestLocked();
        }
    }

    void Clear()
    {
        LOCK(m_mutex);
        m_entries.clear();
        m_order.clear();
        m_latest_hash.SetNull();
    }

private:
    static constexpr size_t MAX_ENTRIES = 128;

    void RecomputeLatestLocked() EXCLUSIVE_LOCKS_REQUIRED(m_mutex)
    {
        m_latest_hash.SetNull();
        int32_t best_height = -1;
        for (const auto& [hash, witness] : m_entries) {
            if (witness.height > best_height) {
                best_height = witness.height;
                m_latest_hash = hash;
            } else if (witness.height == best_height) {
                m_latest_hash = hash;
            }
        }
    }

    mutable Mutex m_mutex;
    std::map<uint256, RuntimeSupplyWitness> m_entries GUARDED_BY(m_mutex);
    std::deque<uint256> m_order GUARDED_BY(m_mutex);
    uint256 m_latest_hash GUARDED_BY(m_mutex);
};

WitnessCache g_witness_cache;

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
    g_witness_cache.Store(std::move(witness));
}

std::optional<RuntimeSupplyWitness> GetRuntimeSupplyWitness(const uint256& block_hash)
{
    return g_witness_cache.Get(block_hash);
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
    return g_witness_cache.GetLatest();
}

void InvalidateRuntimeSupplyWitness(const uint256& block_hash)
{
    g_witness_cache.Invalidate(block_hash);
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
    g_witness_cache.Clear();
}

} // namespace Consensus
