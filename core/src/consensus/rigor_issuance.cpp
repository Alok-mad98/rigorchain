// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/rigor_issuance.h>

#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>

#include <algorithm>

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    // TASK-010 D-7: never invent Bitcoin's 50 COIN. A zero/negative base is
    // a misconfigured chain — fail closed with zero subsidy.
    if (consensusParams.nBlockSubsidyBase <= 0) {
        return 0;
    }
    // TASK-010 R-6: a zero interval is a division by zero, not a "no-halving"
    // policy. Fail closed the same way as a missing base.
    if (consensusParams.nSubsidyHalvingInterval <= 0) {
        return 0;
    }
    // P0-038: genesis issues 0 ORU. Height >= 1 keeps the frozen ORU_INITIAL_BLOCK_SUBSIDY
    // schedule. Do not compensate the removed 1000 at height 1 or later.
    if (nHeight == 0) {
        return Consensus::ORU_GENESIS_BLOCK_SUBSIDY;
    }
    const CAmount base = consensusParams.nBlockSubsidyBase;

    int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;

    // Rigor: optional cap on halving depth (e.g. 1000 → 500 → 250 → 125 ORU subsidy steps then constant; unlimited cumulative ORU).
    if (consensusParams.nBlockSubsidyMaxHalvingShifts >= 0) {
        halvings = std::min(halvings, consensusParams.nBlockSubsidyMaxHalvingShifts);
        return base >> halvings;
    }

    // Bitcoin-style: subsidy halves every interval until the shift is undefined.
    if (halvings >= 64) {
        return 0;
    }
    CAmount nSubsidy = base;
    nSubsidy >>= halvings;
    return nSubsidy;
}

RigorIssuance ComputeRigorIssuance(int height, const Consensus::Params& params)
{
    RigorIssuance out;
    out.epoch = RigorIssuanceEpoch(height);
    out.oru_subsidy = GetBlockSubsidy(height, params);
    out.res_reward = Consensus::GetResMintReward(height);
    return out;
}
