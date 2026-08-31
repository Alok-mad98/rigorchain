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
    out.res_reward_scheduled = Consensus::GetResMintReward(in.height);
    out.res_mint_authorized = out.res_reward_scheduled > 0;
    const bool res_minted = in.res_mint > 0;

    // RFC-017: W(h) is the mint authorization. After RES_REWARD_LAST_EPOCH the draw may
    // still name a winner (T is constant) but GetResMintReward is 0. Requiring a mint
    // then deadlocks every paying block: a mint of 0 is bad-rigor-res-amount, and
    // omitting it was winner-mismatch. Winner-must-mint applies only while W(h) > 0.
    if (out.res_mint_authorized && in.has_winner && !res_minted) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-winner-mismatch");
    }
    // RFC-015 trigger lock:
    // N<CONVERGENCE_DRAW_MIN_TXS_FOR_RES => RES mint MUST NOT exist; otherwise MUST compute winner and then
    // apply winner-coupled mint rule (winner exists => exactly one RES mint,
    // winner none => RES mint must not exist). After schedule end, a winner does not
    // authorize a mint (res_mint_authorized == false).
    if (in.draw_cardinality < Consensus::CONVERGENCE_DRAW_MIN_TXS_FOR_RES && res_minted) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-trigger-underflow");
    }
    if (!in.has_winner && res_minted) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-without-winner");
    }
    if (!out.res_mint_authorized && res_minted) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-amount");
    }

    // RFC-018: claimable ORU = (all ORU fees − RES-transfer burn share) + subsidy.
    out.oru_claimable = (in.fees_oru - in.fee_burn) + out.oru_subsidy;
    if (in.oru_coinbase > out.oru_claimable) {
        LogPrintf("ERROR: ConnectBlock(): ORU coinbase pays too much (actual=%d vs limit=%d fees=%d res_fee_burn=%d)\n",
            in.oru_coinbase, out.oru_claimable, in.fees_oru, in.fee_burn);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-amount");
    }

    const CAmount expected_res = RigorResCoinbaseExpected(in.fees_res, in.res_bootstrap, in.res_mint);
    if (in.res_coinbase != expected_res) {
        LogPrintf("ERROR: ConnectBlock(): RES coinbase mismatch (actual=%d expected=%d nFeesRes=%d bootstrap=%d mint=%d)\n",
            in.res_coinbase, expected_res, in.fees_res, in.res_bootstrap, in.res_mint);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-res-amount");
    }

    // Observer circulating Δ: subsidy minus this block's C(t); RES Δ is the accepted mint.
    out.oru_circulating_delta = out.oru_subsidy - in.converge_sum;
    out.res_delta = in.res_mint;
    return true;
}
