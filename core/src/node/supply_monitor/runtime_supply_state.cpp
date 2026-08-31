// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_state.h>

#include <node/supply_monitor/runtime_supply_invariant_checker.h>

#include <chain.h>
#include <coins.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <validation.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Consensus {
namespace {

CAmount WarmResMintCache(CBlockIndex* tip)
{
    // FENCE-W1: observational path-fold of mint sums. Not spendable balance.
    if (tip == nullptr) return 0;

    std::vector<CBlockIndex*> pending;
    CBlockIndex* cursor = tip;
    while (cursor != nullptr && !cursor->Observational().HasResMintCache()) {
        pending.push_back(cursor);
        cursor = cursor->pprev;
    }

    CAmount running = (cursor != nullptr) ? cursor->Observational().GetChainResMintTotal() : 0;
    if (running < 0 || running > RES_MAX_SUPPLY) return -1;

    for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
        CBlockIndex* block = *it;
        if (block->m_block_res_mint_sum < 0 || block->m_block_res_mint_sum > RES_MAX_SUPPLY) return -1;
        if (running > RES_MAX_SUPPLY - block->m_block_res_mint_sum) return -1;
        running += block->m_block_res_mint_sum;
        block->Observational().WriteResMintCache(running);
    }
    return running;
}

CAmount WarmOruSupplyCache(CBlockIndex* tip, const Params& consensus)
{
    if (tip == nullptr) return 0;

    std::vector<CBlockIndex*> pending;
    CBlockIndex* cursor = tip;
    while (cursor != nullptr && !cursor->Observational().HasOruCache()) {
        pending.push_back(cursor);
        cursor = cursor->pprev;
    }

    CAmount running = (cursor != nullptr) ? cursor->Observational().GetChainOruTotal() : 0;
    if (running < 0) return -1;

    for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
        CBlockIndex* block = *it;
        const CAmount subsidy = GetBlockSubsidy(block->nHeight, consensus);
        const CAmount converge = block->m_block_converge_sum;
        if (converge < 0) return -1;
        running = running + subsidy - converge;
        if (running < 0) return -1;
        block->Observational().WriteOruCache(running);
    }
    return running;
}

CAmount ParentOruSupply(const CBlockIndex* pindex, const Params& consensus)
{
    if (pindex == nullptr || pindex->pprev == nullptr) return 0;
    CBlockIndex* const prev = pindex->pprev;
    if (prev->Observational().HasOruCache()) return prev->Observational().GetChainOruTotal();
    const CAmount warmed = WarmOruSupplyCache(prev, consensus);
    return warmed >= 0 ? warmed : 0;
}

CAmount ParentResMint(const CBlockIndex* pindex)
{
    if (pindex == nullptr || pindex->pprev == nullptr) return 0;
    CBlockIndex* const prev = pindex->pprev;
    if (prev->Observational().HasResMintCache()) return prev->Observational().GetChainResMintTotal();
    const CAmount warmed = WarmResMintCache(prev);
    return warmed >= 0 ? warmed : 0;
}

} // namespace

CAmount ComputeDerivedOruSubsidyCumulative(int height, const Params& consensus)
{
    if (height < 0) return 0;
    const int interval = consensus.nSubsidyHalvingInterval;
    if (interval <= 0) return -1;

    // Closed form: each halving era is constant, so the sum is O(number of eras), not O(height).
    // P0-038: height 0 pays 0; era 0's repeating rate is sampled at height 1, not height 0.
    CAmount sum = 0;
    const CAmount genesis = GetBlockSubsidy(0, consensus);
    if (genesis > 0) {
        if (sum > std::numeric_limits<CAmount>::max() - genesis) return -1;
        sum += genesis;
    }
    if (height == 0) return sum;

    int64_t remaining = static_cast<int64_t>(height); // heights 1..height inclusive
    int era = 0;
    while (remaining > 0) {
        const int sample_height = (era == 0) ? 1
            : (era > (std::numeric_limits<int>::max() / interval)
                ? std::numeric_limits<int>::max()
                : era * interval);
        const CAmount subsidy = GetBlockSubsidy(sample_height, consensus);
        int64_t count;
        if (consensus.nBlockSubsidyMaxHalvingShifts >= 0 &&
            era >= consensus.nBlockSubsidyMaxHalvingShifts) {
            count = remaining;
        } else if (consensus.nBlockSubsidyMaxHalvingShifts < 0 && era >= 64) {
            break;
        } else {
            // Era 0 after genesis covers heights 1 .. interval-1 (interval-1 blocks).
            const int64_t era_len = (era == 0)
                ? static_cast<int64_t>(interval) - 1
                : static_cast<int64_t>(interval);
            if (era_len <= 0) {
                ++era;
                if (era > 128) break;
                continue;
            }
            count = std::min(remaining, era_len);
        }
        if (subsidy > 0) {
            if (count > std::numeric_limits<CAmount>::max() / subsidy) return -1;
            const CAmount chunk = subsidy * count;
            if (sum > std::numeric_limits<CAmount>::max() - chunk) return -1;
            sum += chunk;
        }
        remaining -= count;
        ++era;
        if (era > 128) break;
    }
    return sum;
}

void AggregateUtxoSupplyBalances(const CCoinsViewCache& view,
                                 CAmount& utxo_res_balance,
                                 CAmount& utxo_non_res_balance)
{
    utxo_res_balance = 0;
    utxo_non_res_balance = 0;

    std::unique_ptr<CCoinsViewCursor> pcursor(view.Cursor());
    if (!pcursor) return;

    while (pcursor->Valid()) {
        COutPoint key;
        Coin coin;
        if (pcursor->GetKey(key) && pcursor->GetValue(coin) && !coin.IsSpent()) {
            if (IsResScript(coin.out.scriptPubKey)) {
                utxo_res_balance += coin.out.nValue;
            } else {
                utxo_non_res_balance += coin.out.nValue;
            }
        }
        pcursor->Next();
    }
}

void WarmSupplyLedgerCaches(CBlockIndex* pindex, const Params& consensus)
{
    if (pindex == nullptr) return;
    (void)WarmResMintCache(pindex);
    (void)WarmOruSupplyCache(pindex, consensus);
}

RuntimeSupplyState CaptureRuntimeSupplyState(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus,
                                             const uint8_t res_mint_count_block,
                                             const std::optional<CAmount> oru_supply_parent_opt,
                                             const std::optional<CAmount> res_mint_parent_opt)
{
    assert(pindex != nullptr);

    RuntimeSupplyState out;
    out.height = pindex->nHeight;
    out.block_hash = pindex->GetBlockHash();
    out.parent_hash = pindex->pprev == nullptr ? uint256{} : pindex->pprev->GetBlockHash();

    out.res_max_supply = RES_MAX_SUPPLY;
    // RFC-017: the scheduled reward at this height, which is what RES-L3/RES-L6 compare against.
    out.res_reward_amount = GetResMintReward(pindex->nHeight);

    out.oru_subsidy_block = GetBlockSubsidy(pindex->nHeight, consensus);
    out.oru_converge_burn_block = pindex->m_block_converge_sum;
    out.res_mint_block = pindex->m_block_res_mint_sum;
    out.res_mint_count_block = res_mint_count_block > 1 ? 1 : res_mint_count_block;

    WarmSupplyLedgerCaches(const_cast<CBlockIndex*>(pindex), consensus);

    out.oru_supply_cumulative = pindex->Observational().GetChainOruTotal();
    out.res_mint_cumulative = pindex->Observational().GetChainResMintTotal();

    out.oru_supply_parent = oru_supply_parent_opt.has_value()
                                ? *oru_supply_parent_opt
                                : ParentOruSupply(pindex, consensus);
    out.res_mint_parent = res_mint_parent_opt.has_value() ? *res_mint_parent_opt : ParentResMint(pindex);

    // Do not scan the UTXO set here (RIG-01). ConnectBlock used to call
    // AggregateUtxoSupplyBalances on a CCoinsViewCache wrapping CoinsTip(), which
    // materialized the entire UTXO set on every block. UTXO aggregates remain
    // diagnostic: callers that need them (RPC) pass them in or call
    // AggregateUtxoSupplyBalances on CoinsDB() themselves.
    (void)view;

    const CAmount derived = ComputeDerivedOruSubsidyCumulative(pindex->nHeight, consensus);
    if (derived >= 0) {
        out.oru_subsidy_cumulative_derived = derived;
    }

    return out;
}

void StoreRuntimeSupplyMirror(CBlockIndex* pindex, RuntimeSupplyState state)
{
    assert(pindex != nullptr);
    pindex->m_runtime_supply_mirror = std::move(state);
}

void InvalidateRuntimeSupplyMirror(const CBlockIndex* pindex)
{
    if (pindex == nullptr) return;
    pindex->m_runtime_supply_mirror.reset();
}

const std::optional<RuntimeSupplyState>& GetRuntimeSupplyMirror(const CBlockIndex& pindex)
{
    return pindex.m_runtime_supply_mirror;
}

std::vector<std::string> CheckRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                      const Params& consensus)
{
    std::vector<std::string> errors = VerifyRuntimeSupplyInvariants(state, consensus).violations;
    if (state.utxo_res_balance > state.res_mint_cumulative) {
        errors.push_back("UTXO-2");
    }
    return errors;
}

} // namespace Consensus
