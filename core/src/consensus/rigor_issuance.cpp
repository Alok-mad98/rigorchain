// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/rigor_issuance.h>

#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    // RFC-020: subsidy is the lock constant, not a function of Params.
    (void)consensusParams;
    if (nHeight < 0) {
        return 0;
    }
    if (nHeight == 0) {
        return Consensus::ORU_GENESIS_BLOCK_SUBSIDY;
    }
    return Consensus::ORU_FLAT_BLOCK_SUBSIDY;
}

RigorIssuance ComputeRigorIssuance(int height, const Consensus::Params& params)
{
    RigorIssuance out;
    out.epoch = RigorIssuanceEpoch(height);
    out.oru_subsidy = GetBlockSubsidy(height, params);
    return out;
}
