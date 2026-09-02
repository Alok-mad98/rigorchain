// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Genesis production Kernel. Calls existing unique authorities only.
// Does not recode issuance, RES, draw, classify, or supply.

#include <consensus/kernel/genesis_authority.h>

#include <consensus/rigor_res.h>

ConvergenceDrawOutcome RigorConsensusKernel::ResolveDraw(const ConvergenceDrawSet& F, const uint256& T)
{
    return ResolveConvergenceDraw(F, T);
}

RigorIssuance RigorConsensusKernel::Issuance(int height, const Consensus::Params& params)
{
    return ComputeRigorIssuance(height, params);
}

AssetType RigorConsensusKernel::Classify(const CScript& script)
{
    return ClassifyRigorAsset(script);
}

bool RigorConsensusKernel::CoinbaseMature(int spend_height, int coin_height, int64_t maturity)
{
    if (coin_height < 0) return true;
    return static_cast<int64_t>(spend_height) - static_cast<int64_t>(coin_height) >= maturity;
}

bool RigorConsensusKernel::EvaluateConnect(const RigorConsensusKernelConnectInput& in,
    RigorConsensusKernelConnectResult& out, BlockValidationState& state)
{
    out = RigorConsensusKernelConnectResult{};
    if (!in.coinbase || !in.params) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "kernel-connect-input");
    }
    if (in.height < 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "kernel-negative-height");
    }

    if (!rigor::ConnectEvaluateCoinbaseResMint(*in.coinbase, in.height, *in.params,
            in.parent_draw, in.parent_draw_outcome, state, out.res_mint)) {
        return false;
    }

    RigorSupplyDeltaInput supply = in.supply;
    supply.height = in.height;
    supply.res_mint = out.res_mint;
    if (!RigorSupplyDelta(supply, *in.params, out.supply, state)) {
        return false;
    }
    out.accept = true;
    return true;
}
