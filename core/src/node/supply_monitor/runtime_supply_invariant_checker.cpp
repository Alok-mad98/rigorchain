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

    // RES-L4: M(h) ∈ {0,1}. An indicator under RFC-019, so this is a mirror-integrity
    // check, not the retired "one winner per block" rule.
    if (state.res_mint_present_block != 0 && state.res_mint_present_block != 1) {
        fail("RES-L4");
    }

    // RES-L5
    if (state.res_mint_cumulative != state.res_mint_parent + state.res_mint_block) {
        fail("RES-L5");
    }

    // RES-L6: RES appears at this height iff the block minted.
    //
    // This is the inflation-facing direction. Without it a mirror could report new RES
    // with no minting transaction behind it and every other invariant would still hold:
    // L5 only ties the running total to res_mint_block, and L2 only caps it.
    //
    // The pre-RFC-019 form compared res_mint_block against the scheduled reward
    // res_reward_amount. RFC-019 has no schedule to compare against — the amount is
    // whatever the burns bought at the epoch price — so the comparison is against
    // presence instead. `!= 0` rather than `> 0` so a negative amount also trips it.
    if ((state.res_mint_block != 0) != (state.res_mint_present_block == 1)) {
        fail("RES-L6");
    }

    return result;
}

} // namespace Consensus
