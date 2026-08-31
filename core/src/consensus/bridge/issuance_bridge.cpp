// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Calls ComputeRigorIssuance only. Does not recode the schedule.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/rigor_issuance.h>

bool rigor_compute_issuance(uint32_t height, rigor_issuance_result* out)
{
    if (!out) return false;
    const RigorIssuance g = ComputeRigorIssuance(static_cast<int>(height), KernelFrozenParams());
    out->oru_subsidy = static_cast<uint64_t>(g.oru_subsidy);
    out->res_reward_schedule = static_cast<uint64_t>(g.res_reward);
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
