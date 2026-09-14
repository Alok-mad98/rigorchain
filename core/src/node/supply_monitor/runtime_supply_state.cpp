// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_state.h>

#include <node/supply_monitor/observational_metrics.h>
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
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Consensus {
namespace {

std::optional<CAmount> ParentOruSupply(const CBlockIndex* pindex, const Params& consensus)
{
    if (pindex == nullptr || pindex->pprev == nullptr) return 0;
    const ObservationalFoldResult fold = FoldChainOruTotal(pindex->pprev, consensus);
    if (!fold.ok()) return std::nullopt;
    return fold.value;
}

std::optional<CAmount> ParentResMint(const CBlockIndex* pindex)
{
    if (pindex == nullptr || pindex->pprev == nullptr) return 0;
    const ObservationalFoldResult fold = FoldChainResMintTotal(pindex->pprev);
    if (!fold.ok()) return std::nullopt;
    return fold.value;
}

} // namespace

CAmount ComputeDerivedOruSubsidyCumulative(int height, const Params& consensus)
{
    (void)consensus;
    if (height <= 0) return 0;
    // RFC-020 closed form: height 0 pays 0; every h >= 1 pays ORU_FLAT_BLOCK_SUBSIDY.
    // Σ_{i=1}^{height} 128 ORU = height * ORU_FLAT_BLOCK_SUBSIDY.
    const CAmount flat = ORU_FLAT_BLOCK_SUBSIDY;
    if (flat <= 0) return -1;
    if (height > std::numeric_limits<CAmount>::max() / flat) return -1;
    return static_cast<CAmount>(height) * flat;
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
    (void)FoldChainResMintTotal(pindex);
    (void)FoldChainOruTotal(pindex, consensus);
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
    out.oru_subsidy_block = GetBlockSubsidy(pindex->nHeight, consensus);
    out.oru_converge_burn_block = pindex->m_block_converge_sum;
    out.res_mint_block = pindex->m_block_res_mint_sum;
    // Presence, not cardinality: RFC-020 allows many mints in one block.
    out.res_mint_present_block = res_mint_count_block > 0 ? 1 : 0;

    WarmSupplyLedgerCaches(const_cast<CBlockIndex*>(pindex), consensus);

    out.oru_supply_cumulative = pindex->Observational().GetChainOruTotal();
    out.res_mint_cumulative = pindex->Observational().GetChainResMintTotal();

    if (oru_supply_parent_opt.has_value()) {
        out.oru_supply_parent = *oru_supply_parent_opt;
    } else if (const std::optional<CAmount> parent = ParentOruSupply(pindex, consensus)) {
        out.oru_supply_parent = *parent;
    } else {
        // Fold Invalid: do not invent 0. Cumulative stays uncached (-1) so RES-L1 / ORU-L1 fail.
        out.oru_supply_parent = 0;
    }
    if (res_mint_parent_opt.has_value()) {
        out.res_mint_parent = *res_mint_parent_opt;
    } else if (const std::optional<CAmount> parent = ParentResMint(pindex)) {
        out.res_mint_parent = *parent;
    } else {
        out.res_mint_parent = 0;
    }

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
