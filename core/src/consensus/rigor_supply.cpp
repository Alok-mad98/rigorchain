// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/rigor_supply.h>

#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/validation.h>
#include <logging.h>

bool RigorSupplyDelta(const RigorSupplyDeltaInput& in, const Consensus::Params& params,
                      RigorSupplyDeltaResult& out, BlockValidationState& state)
{
    out = {};
    out.oru_subsidy = GetBlockSubsidy(in.height, params);

    // RFC-018: claimable ORU = (all ORU fees − RES-transfer burn share) + subsidy.
    out.oru_claimable = (in.fees_oru - in.fee_burn) + out.oru_subsidy;
    if (in.oru_coinbase > out.oru_claimable) {
        LogPrintf("ERROR: ConnectBlock(): ORU coinbase pays too much (actual=%d vs limit=%d fees=%d res_fee_burn=%d)\n",
            in.oru_coinbase, out.oru_claimable, in.fees_oru, in.fee_burn);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-amount");
    }

    const CAmount expected_res = RigorResCoinbaseExpected(in.fees_res, in.res_bootstrap);
    if (in.res_coinbase != expected_res) {
        LogPrintf("ERROR: ConnectBlock(): RES coinbase mismatch (actual=%d expected=%d nFeesRes=%d bootstrap=%d)\n",
            in.res_coinbase, expected_res, in.fees_res, in.res_bootstrap);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-res-amount");
    }

    // Observer circulating Δ: subsidy minus this block's C(t); RES Δ is what the body minted.
    out.oru_circulating_delta = out.oru_subsidy - in.converge_sum;
    out.res_delta = in.res_mint;
    return true;
}
