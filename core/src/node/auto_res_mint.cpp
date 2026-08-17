// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/auto_res_mint.h>

#include <chainparams.h>
#include <coins.h>
#include <consensus/convergenceres.h>
#include <consensus/res_mint_witness.h>
#include <consensus/res_owner.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/amount.h>
#include <hash.h>
#include <logging.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>
#include <util/moneystr.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <optional>
#include <vector>

namespace {

static constexpr CAmount kAutoMintOruFee{1000};

/** P2WSH redeem for devtest auto-mint funding: drop PCORB witness pushes (see `BuildPcorbBodyResMintWitness`), then OP_TRUE. */
const CScript& RedeemAutoResMintBootstrapScript()
{
    static const CScript k = [] {
        CScript s;
        for (size_t i = 0; i < PCORB_BODY_RES_MINT_WITNESS_ITEMS; ++i) {
            s << OP_DROP;
        }
        s << OP_TRUE;
        return s;
    }();
    return k;
}

} // namespace

const CScript& AutoResMintBootstrapScriptPubKey()
{
    static const CScript k = [] {
        const CScript& redeem = RedeemAutoResMintBootstrapScript();
        return GetScriptForDestination(WitnessV0ScriptHash(redeem));
    }();
    return k;
}

void AppendAutoResMintBootstrapCoinbaseVout(const CChainParams& params, int n_height, CMutableTransaction& coinbase)
{
    if (!params.IsMockableChain()) return;
    if (n_height != 1) return;
    if (coinbase.vout.empty()) return;
    static constexpr CAmount k_fund = 50 * COIN;
    if (coinbase.vout[0].nValue < k_fund) {
        return;
    }
    coinbase.vout[0].nValue -= k_fund;
    CTxOut boot;
    boot.nValue = k_fund;
    boot.scriptPubKey = AutoResMintBootstrapScriptPubKey();
    coinbase.vout.push_back(std::move(boot));
}

std::optional<CMutableTransaction> TryCreateAutomaticResMintTx(const ConvergenceDrawSet& parent_draw_set,
    const ConvergenceDrawOutcome& parent_outcome,
    int block_height,
    const CCoinsViewCache& assemble_view,
    const CScript& mining_res_receive_script)
{
    if (!parent_outcome.HasWinner() || parent_draw_set.source_height + 1 != block_height) {
        LogPrintf("TryCreateAutomaticResMintTx: skip (has_winner=%d source_height=%d payout_height=%d)\n",
                  parent_outcome.HasWinner() ? 1 : 0, parent_draw_set.source_height, block_height);
        return std::nullopt;
    }
    // Non-empty `mining_res_receive_script` now only signals that this node is configured to
    // assemble RES mints; since A-5 it is no longer the payout destination.
    if (mining_res_receive_script.empty()) {
        LogPrintf("TryCreateAutomaticResMintTx: mint assembly is not armed (empty script) height=%d\n", block_height);
        return std::nullopt;
    }

    const ConvergenceDrawCandidate& winner =
        parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)];
    const CAmount conv_amt = static_cast<CAmount>(winner.weight);
    if (conv_amt < Consensus::MIN_CONVERGENCE_AMOUNT || !MoneyRange(conv_amt)) {
        LogPrintf("TryCreateAutomaticResMintTx: winner weight out of range txid=%s weight=%s\n",
                  winner.txid.ToString(), FormatMoney(conv_amt));
        return std::nullopt;
    }

    // SECURITY-001-A / A-5 R2: the payout target is the winner's declaration, not this
    // node's configured address. A template built with any other script is rejected by
    // ConnectBlock with bad-rigor-res-owner-mismatch.
    if (winner.res_payout_program32.size() != 32) {
        LogPrintf("TryCreateAutomaticResMintTx: winner %s has no 32-byte RES owner program (len=%u)\n",
                  winner.txid.ToString(), static_cast<unsigned>(winner.res_payout_program32.size()));
        return std::nullopt;
    }
    const CScript res_payout_script = CScript() << OP_1 << winner.res_payout_program32;

    CScriptWitness wit = BuildPcorbBodyResMintWitness(winner.txid,
        conv_amt,
        parent_draw_set.source_block_hash,
        static_cast<uint64_t>(block_height),
        parent_draw_set.source_nbits,
        parent_draw_set.convergence_root);
    if (wit.IsNull()) {
        LogPrintf("TryCreateAutomaticResMintTx: PCORB witness build returned null for winner %s\n",
                  winner.txid.ToString());
        return std::nullopt;
    }
    const CScript& redeem = RedeemAutoResMintBootstrapScript();
    wit.stack.emplace_back(redeem.begin(), redeem.end());

    std::vector<std::pair<COutPoint, CAmount>> spendable;
    for (std::unique_ptr<CCoinsViewCursor> cur(assemble_view.Cursor()); cur && cur->Valid(); cur->Next()) {
        COutPoint op;
        Coin coin;
        if (!cur->GetKey(op) || !cur->GetValue(coin)) continue;
        if (coin.IsSpent()) continue;
        if (coin.out.scriptPubKey != AutoResMintBootstrapScriptPubKey()) continue;
        spendable.emplace_back(op, coin.out.nValue);
    }
    if (spendable.empty()) {
        LogPrintf("TryCreateAutomaticResMintTx: no AutoResMintBootstrap P2WSH UTXO at height=%d; "
                  "fund via getautomintfundaddress\n",
                  block_height);
        return std::nullopt;
    }
    std::sort(spendable.begin(), spendable.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    const COutPoint fund_prev = spendable.front().first;
    const CAmount in_amt = spendable.front().second;
    if (!MoneyRange(in_amt) || in_amt <= kAutoMintOruFee) {
        LogPrintf("TryCreateAutomaticResMintTx: bootstrap UTXO %s:%u value %s is too small (need > %s)\n",
                  fund_prev.hash.ToString(), fund_prev.n, FormatMoney(in_amt), FormatMoney(kAutoMintOruFee));
        return std::nullopt;
    }
    const CAmount oru_change = in_amt - kAutoMintOruFee;
    if (!MoneyRange(oru_change)) {
        LogPrintf("TryCreateAutomaticResMintTx: ORU change out of range in=%s fee=%s\n",
                  FormatMoney(in_amt), FormatMoney(kAutoMintOruFee));
        return std::nullopt;
    }

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout = fund_prev;
    mtx.vin[0].scriptWitness = std::move(wit);

    mtx.vout.resize(2);
    mtx.vout[0].nValue = Consensus::RES_REWARD_AMOUNT;
    mtx.vout[0].scriptPubKey = res_payout_script;
    mtx.vout[1].nValue = oru_change;
    mtx.vout[1].scriptPubKey = AutoResMintBootstrapScriptPubKey();

    return mtx;
}
