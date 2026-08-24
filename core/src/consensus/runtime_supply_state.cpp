// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_state.h>

#include <consensus/runtime_supply_invariant_checker.h>

#include <chain.h>
#include <coins.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <validation.h>

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
    // MoneyRange() caps at MAX_MONEY (21M ORU), which bounds a single output, not a chain
    // aggregate. ORU has tail emission and its cumulative subsidy passes 21M around height
    // 21,000, so gating this sum on MoneyRange() would return -1 for every block thereafter.
    // The real bound here is int64 overflow.
    CAmount sum = 0;
    for (int h = 0; h <= height; ++h) {
        const CAmount subsidy = GetBlockSubsidy(h, consensus);
        if (subsidy < 0 || sum > std::numeric_limits<CAmount>::max() - subsidy) return -1;
        sum += subsidy;
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

    AggregateUtxoSupplyBalances(view, out.utxo_res_balance, out.utxo_non_res_balance);

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
