// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/auto_res_mint.h>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/res_owner.h>
#include <consensus/rigor_consensus_lock.h>
#include <logging.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <util/moneystr.h>

bool TryAppendCoinbaseResMint(CMutableTransaction& coinbase,
    const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    int block_height)
{
    if (!parent_outcome.HasWinner() || parent_draw_set.source_height + 1 != block_height) {
        LogPrintf("TryAppendCoinbaseResMint: skip (has_winner=%d source_height=%d payout_height=%d)\n",
                  parent_outcome.HasWinner() ? 1 : 0, parent_draw_set.source_height, block_height);
        return false;
    }

    const CAmount res_reward = Consensus::GetResMintReward(block_height);
    if (res_reward <= 0) {
        LogPrintf("TryAppendCoinbaseResMint: RES issuance schedule exhausted at height=%d\n", block_height);
        return false;
    }

    const ConvergenceDrawCandidate& winner =
        parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)];
    const CAmount conv_amt = static_cast<CAmount>(winner.weight);
    if (conv_amt < Consensus::MIN_CONVERGENCE_AMOUNT || !MoneyRange(conv_amt)) {
        LogPrintf("TryAppendCoinbaseResMint: winner weight out of range txid=%s weight=%s\n",
                  winner.txid.ToString(), FormatMoney(conv_amt));
        return false;
    }
    if (winner.res_payout_hash160.IsNull()) {
        LogPrintf("TryAppendCoinbaseResMint: winner %s has a null RES owner hash\n",
                  winner.txid.ToString());
        return false;
    }

    const int64_t maturity_height =
        static_cast<int64_t>(block_height) + Params().GetConsensus().nCoinbaseMaturity;
    coinbase.vout.emplace_back(res_reward,
        GetResMintScript(maturity_height, winner.res_payout_hash160));
    return true;
}
