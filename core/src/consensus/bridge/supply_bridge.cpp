// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Calls RigorSupplyDelta / RigorResCoinbaseExpected only.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>

bool rigor_supply_delta(const rigor_supply_input* input, rigor_supply_result* output)
{
    if (!input || !output) return false;

    RigorSupplyDeltaInput in;
    in.height = static_cast<int>(input->height);
    in.fees_oru = static_cast<CAmount>(input->fees_oru);
    in.fee_burn = static_cast<CAmount>(input->burn);
    in.converge_sum = static_cast<CAmount>(input->converge);
    in.res_mint = static_cast<CAmount>(input->mint);
    in.oru_coinbase = static_cast<CAmount>(input->oru_coinbase);
    in.res_bootstrap = 0;
    in.fees_res = 0;
    in.res_coinbase = RigorResCoinbaseExpected(in.fees_res, in.res_bootstrap);

    BlockValidationState state;
    RigorSupplyDeltaResult out;
    if (!RigorSupplyDelta(in, KernelFrozenParams(), out, state)) {
        output->delta = 0;
        return false;
    }
    output->delta = static_cast<int64_t>(out.oru_circulating_delta);
    return true;
}
