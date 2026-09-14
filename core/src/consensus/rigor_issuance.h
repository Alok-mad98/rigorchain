// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus Kernel — issuance. ORU is flat 128 for every height ≥ 1 (RFC-020).
// The observational epoch is still floor(height / 210000). RES arithmetic lives
// in res_issuance.h and no longer reads that epoch.

#ifndef BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H
#define BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>

namespace Consensus {
struct Params;
}

/** Height → observational epoch. Not a mint or subsidy argument. */
[[nodiscard]] inline int RigorIssuanceEpoch(int height)
{
    if (height < 0) return 0;
    return height / Consensus::RES_REWARD_EPOCH;
}

struct RigorIssuance {
    CAmount oru_subsidy{0};
    int epoch{0};
};

/** ORU subsidy and epoch at `height`. Genesis applies 0 ORU (P0-038). */
RigorIssuance ComputeRigorIssuance(int height, const Consensus::Params& params);

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

#endif // BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H
