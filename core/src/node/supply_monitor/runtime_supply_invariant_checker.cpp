// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_invariant_checker.h>

#include <consensus/params.h>
#include <validation.h>

#include <utility>

namespace Consensus {

RuntimeInvariantResult VerifyRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                     const Params& consensus)
{
    RuntimeInvariantResult result;
    result.checked_invariants = RUNTIME_SUPPLY_INVARIANT_COUNT;

    auto fail = [&](const char* id) {
        result.passed = false;
        result.violations.push_back(id);
    };

    // ORU-L1
    if (state.oru_supply_cumulative < 0) {
        fail("ORU-L1");
    }

    // ORU-L2
    if (state.oru_converge_burn_block < 0) {
        fail("ORU-L2");
    }

    // ORU-L3: S_ORU(h) = S_ORU(h-1) + R(h) - C(h)
    if (state.oru_supply_cumulative !=
        state.oru_supply_parent + state.oru_subsidy_block - state.oru_converge_burn_block) {
        fail("ORU-L3");
    }

    // ORU-L4: R(h) = GetBlockSubsidy(height)
    if (state.height < 0) {
        fail("ORU-L4");
    } else if (state.oru_subsidy_block != GetBlockSubsidy(state.height, consensus)) {
        fail("ORU-L4");
    }

    // RES-L1
    if (state.res_mint_cumulative < 0) {
        fail("RES-L1");
    }

    // RES-L2
    if (state.res_mint_cumulative > state.res_max_supply) {
        fail("RES-L2");
    }

    // RES-L3
    if (state.res_mint_block != 0 && state.res_mint_block != state.res_reward_amount) {
        fail("RES-L3");
    }

    // RES-L4: M(h) ∈ {0,1}
    if (state.res_mint_count_block != 0 && state.res_mint_count_block != 1) {
        fail("RES-L4");
    }

    // RES-L5
    if (state.res_mint_cumulative != state.res_mint_parent + state.res_mint_block) {
        fail("RES-L5");
    }

    // RES-L6: mint amount present iff M(h) = 1
    const bool minted = state.res_mint_block == state.res_reward_amount;
    const bool count_one = state.res_mint_count_block == 1;
    if (minted != count_one) {
        fail("RES-L6");
    }

    return result;
}

} // namespace Consensus
