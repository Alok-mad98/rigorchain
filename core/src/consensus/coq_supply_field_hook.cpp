// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/coq_supply_field_hook.h>

#include <chain.h>

#include <string>
#include <utility>
#include <vector>

namespace Consensus {

CoqSupplyFieldHook ProjectCoqSupplyFieldHook(const RuntimeSupplyState& mirror)
{
    CoqSupplyFieldHook hook;
    hook.height = mirror.height;
    hook.block_hash = mirror.block_hash;
    hook.parent_hash = mirror.parent_hash;

    hook.rs_oru_supply = mirror.oru_supply_cumulative;
    hook.rs_res_supply = mirror.res_mint_cumulative;

    hook.cb_oru_subsidy = mirror.oru_subsidy_block;
    hook.cb_oru_burn_delta = mirror.oru_converge_burn_block;
    hook.cb_res_mint_delta = mirror.res_mint_block;

    hook.res_mint_count_M_h = mirror.res_mint_count_block;

    hook.rs_oru_supply_parent = mirror.oru_supply_parent;
    hook.rs_res_supply_parent = mirror.res_mint_parent;

    hook.audit_oru_subsidy_cumulative_derived = mirror.oru_subsidy_cumulative_derived;
    hook.audit_coq_cb_oru_mint_delta_scaffold = mirror.oru_subsidy_block;

    return hook;
}

std::vector<std::string> CheckCoqHookMatchesAuthoritativeCaches(const CoqSupplyFieldHook& hook,
                                                                const RuntimeSupplyState& mirror,
                                                                const CBlockIndex& pindex)
{
    std::vector<std::string> errors;

    auto fail = [&](const std::string& msg) { errors.push_back(msg); };

    if (hook.height != pindex.nHeight) {
        fail("MIRROR-1: hook.height != pindex.nHeight");
    }
    if (hook.block_hash != pindex.GetBlockHash()) {
        fail("MIRROR-1: hook.block_hash != pindex.GetBlockHash()");
    }

    if (hook.rs_oru_supply != mirror.oru_supply_cumulative) {
        fail("Coq hook rs_oru_supply != mirror.oru_supply_cumulative");
    }
    if (hook.rs_res_supply != mirror.res_mint_cumulative) {
        fail("Coq hook rs_res_supply != mirror.res_mint_cumulative");
    }
    if (hook.cb_oru_subsidy != mirror.oru_subsidy_block) {
        fail("Coq hook cb_oru_subsidy != mirror.oru_subsidy_block");
    }
    if (hook.cb_oru_burn_delta != mirror.oru_converge_burn_block) {
        fail("Coq hook cb_oru_burn_delta != mirror.oru_converge_burn_block");
    }
    if (hook.cb_res_mint_delta != mirror.res_mint_block) {
        fail("Coq hook cb_res_mint_delta != mirror.res_mint_block");
    }
    if (hook.res_mint_count_M_h != mirror.res_mint_count_block) {
        fail("Coq hook res_mint_count_M_h != mirror.res_mint_count_block");
    }

    if (pindex.Observational().HasOruCache() && hook.rs_oru_supply != pindex.Observational().GetChainOruTotal()) {
        fail("Coq hook rs_oru_supply != ObservationalMetrics::GetChainOruTotal()");
    }
    if (hook.cb_oru_burn_delta != pindex.m_block_converge_sum) {
        fail("Coq hook cb_oru_burn_delta != pindex.m_block_converge_sum");
    }
    if (pindex.Observational().HasResMintCache() && hook.rs_res_supply != pindex.Observational().GetChainResMintTotal()) {
        fail("Coq hook rs_res_supply != ObservationalMetrics::GetChainResMintTotal()");
    }
    if (hook.cb_res_mint_delta != pindex.m_block_res_mint_sum) {
        fail("Coq hook cb_res_mint_delta != pindex.m_block_res_mint_sum");
    }

    return errors;
}

CoqSupplyFieldHook ResolveCoqSupplyFieldHook(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus)
{
    assert(pindex != nullptr);

    WarmSupplyLedgerCaches(const_cast<CBlockIndex*>(pindex), consensus);

    const std::optional<RuntimeSupplyState>& stored = GetRuntimeSupplyMirror(*pindex);
    if (stored.has_value() && stored->block_hash == pindex->GetBlockHash()) {
        return ProjectCoqSupplyFieldHook(*stored);
    }

    const uint8_t mint_count = pindex->m_block_res_mint_sum > 0 ? 1 : 0;
    const RuntimeSupplyState fresh =
        CaptureRuntimeSupplyState(pindex, view, consensus, mint_count);
    return ProjectCoqSupplyFieldHook(fresh);
}

} // namespace Consensus
