// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus Kernel — issuance. One epoch formula: floor(height / 210000).
// Does not redefine the schedule; ORU uses GetBlockSubsidy, RES uses GetResMintReward.

#ifndef BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H
#define BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>

namespace Consensus {
struct Params;
}

/** Height → epoch. Same division as GetResMintReward / ORU halvings. */
[[nodiscard]] inline int RigorIssuanceEpoch(int height)
{
    if (height < 0) return 0;
    return height / Consensus::RES_REWARD_EPOCH;
}

struct RigorIssuance {
    CAmount oru_subsidy{0};
    CAmount res_reward{0};
    int epoch{0};
};

/** Schedule values at `height`. Genesis applies 0 ORU; RES schedule at 0 is W(0). */
RigorIssuance ComputeRigorIssuance(int height, const Consensus::Params& params);

CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

#endif // BITCOIN_CONSENSUS_RIGOR_ISSUANCE_H
