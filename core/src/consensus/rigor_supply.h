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

/** RES coinbase identity: fees + bootstrap. RFC-019 leaves no mint term — issuance
 *  is a body transaction, so a coinbase that pays RES it did not collect is invalid. */
[[nodiscard]] inline CAmount RigorResCoinbaseExpected(CAmount fees_res, CAmount bootstrap)
{
    return fees_res + bootstrap;
}

struct RigorSupplyDeltaInput {
    int height{0};
    CAmount fees_oru{0};
    CAmount fee_burn{0};
    CAmount fees_res{0};
    CAmount oru_coinbase{0};
    CAmount res_coinbase{0};
    /** RFC-019: RES minted by this block's body transactions, each priced against the
     *  epoch by CheckTxInputs. Observational only here — the gate is per transaction. */
    CAmount res_mint{0};
    CAmount res_bootstrap{0};
    CAmount converge_sum{0};
};

struct RigorSupplyDeltaResult {
    CAmount oru_subsidy{0};
    CAmount oru_claimable{0};
    CAmount oru_circulating_delta{0};
    CAmount res_delta{0};
};

/**
 * Coinbase ORU/RES budgets and the circulating Δ.
 * Reject reasons match ConnectBlock: bad-cb-amount, bad-cb-res-amount.
 */
bool RigorSupplyDelta(const RigorSupplyDeltaInput& in, const Consensus::Params& params,
                      RigorSupplyDeltaResult& out, BlockValidationState& state);

#endif // BITCOIN_CONSENSUS_RIGOR_SUPPLY_H
