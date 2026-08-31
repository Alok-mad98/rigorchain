// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Consensus Kernel — monetary slice formerly inline in ConnectBlock.
// No UTXO, window assembly, or chain access.

#ifndef BITCOIN_CONSENSUS_RIGOR_SUPPLY_H
#define BITCOIN_CONSENSUS_RIGOR_SUPPLY_H

#include <consensus/amount.h>

class BlockValidationState;

namespace Consensus {
struct Params;
}

/** RES coinbase identity: fees + bootstrap + accepted mint. */
[[nodiscard]] inline CAmount RigorResCoinbaseExpected(CAmount fees_res, CAmount bootstrap, CAmount mint)
{
    return fees_res + bootstrap + mint;
}

struct RigorSupplyDeltaInput {
    int height{0};
    CAmount fees_oru{0};
    CAmount fee_burn{0};
    CAmount fees_res{0};
    CAmount oru_coinbase{0};
    CAmount res_coinbase{0};
    CAmount res_mint{0};
    CAmount res_bootstrap{0};
    CAmount converge_sum{0};
    bool has_winner{false};
    int draw_cardinality{0};
};

struct RigorSupplyDeltaResult {
    CAmount oru_subsidy{0};
    CAmount res_reward_scheduled{0};
    CAmount oru_claimable{0};
    CAmount oru_circulating_delta{0};
    CAmount res_delta{0};
    bool res_mint_authorized{false};
};

/**
 * Coinbase ORU/RES budgets, winner-must-mint gates, and the circulating Δ.
 * Reject reasons match ConnectBlock: bad-cb-amount, bad-cb-res-amount,
 * bad-rigor-res-winner-mismatch, bad-rigor-res-trigger-underflow,
 * bad-rigor-res-without-winner, bad-rigor-res-amount.
 */
bool RigorSupplyDelta(const RigorSupplyDeltaInput& in, const Consensus::Params& params,
                      RigorSupplyDeltaResult& out, BlockValidationState& state);

#endif // BITCOIN_CONSENSUS_RIGOR_SUPPLY_H
