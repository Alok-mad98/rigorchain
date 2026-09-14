// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Calls ComputeRigorIssuance and ResDestroyCostAtHeight. Does not recode
// the schedule. RFC-019: the ABI field is the destroy cost, not W(h).

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_issuance.h>

bool rigor_compute_issuance(uint32_t height, rigor_issuance_result* out)
{
    if (!out) return false;
    const int h = static_cast<int>(height);
    const RigorIssuance g = ComputeRigorIssuance(h, KernelFrozenParams());
    out->oru_subsidy = static_cast<uint64_t>(g.oru_subsidy);
    out->res_destroy_cost = static_cast<uint64_t>(Consensus::ResDestroyCostAtHeight(h));
    out->epoch = static_cast<uint32_t>(g.epoch);
    return true;
}

bool rigor_coinbase_mature(uint32_t spend_height, uint32_t coin_height, uint8_t* eligible)
{
    if (!eligible) return false;
    const Consensus::Params p = KernelFrozenParams();
    const int spend = static_cast<int>(spend_height);
    const int coin = static_cast<int>(coin_height);
    *eligible = (spend - coin >= p.nCoinbaseMaturity) ? 1 : 0;
    return true;
}

/** RFC-019 retired the lottery. The ABI entry stays so existing CLIs
 *  keep linking; it always reports no winner. */
bool rigor_resolve_draw(
    const rigor_candidate* candidates,
    size_t count,
    const rigor_draw_context* ctx,
    rigor_draw_result* result)
{
    (void)candidates;
    (void)count;
    (void)ctx;
    if (!result) return false;
    result->has_winner = 0;
    return true;
}
