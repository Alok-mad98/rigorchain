// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Genesis production Kernel. Calls existing unique authorities only.
// Does not recode issuance, RES, draw, classify, or supply.

#include <consensus/kernel/genesis_authority.h>


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

    RigorSupplyDeltaInput supply = in.supply;
    supply.height = in.height;
    if (!RigorSupplyDelta(supply, *in.params, out.supply, state)) {
        return false;
    }
    out.accept = true;
    return true;
}
