// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/tx_semantic.h>

#include <coins.h>
#include <consensus/convergenceres.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/statetag.h>
#include <script/interpreter.h>
#include <script/standard.h>

namespace wallet {

namespace {

bool TxHasResClaimOutput(const CTransaction& tx)
{
    for (const CTxOut& out : tx.vout) {
        std::vector<unsigned char> program;
        int witness_version = 1;
        if (!out.scriptPubKey.IsWitnessProgram(witness_version, program)) continue;
        if (program.size() != WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE) continue;
        return true;
    }
    return false;
}

bool BuildWalletInputView(const CWallet& wallet, const CWalletTx& wtx, CCoinsViewCache& inputs)
{
    AssertLockHeld(wallet.cs_wallet);
    for (const CTxIn& txin : wtx.tx->vin) {
        const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
        if (!prev || txin.prevout.n >= prev->tx->vout.size()) {
            return false;
        }
        int height = 0;
        if (const auto* conf = prev->state<TxStateConfirmed>()) {
            height = conf->confirmed_block_height;
        }
        Coin coin(prev->tx->vout[txin.prevout.n], height, prev->IsCoinBase());
        inputs.AddCoin(txin.prevout, std::move(coin), false);
    }
    return true;
}

} // namespace

TxSemanticKind ClassifyTxSemanticFromOutputs(const CTransaction& tx)
{
    // Never infer ResMint here: a full-balance RES send of RES_REWARD_AMOUNT + ORU
    // change has the same output shape as a body mint. ResMint (and maturity) require
    // ClassifyTxSemantic with input prevouts. Incoming payments only need a UI label.
    bool has_res{false};
    bool has_oru{false};
    for (const CTxOut& out : tx.vout) {
        const AssetType a = AssetTypeFromScriptPubKey(out.scriptPubKey);
        if (a == ASSET_RES) has_res = true;
        else if (a == ASSET_ORU) has_oru = true;
    }
    if (has_res) {
        return TxSemanticKind::ResTransfer;
    }
    if (has_oru) {
        return TxSemanticKind::OruTransfer;
    }
    return TxSemanticKind::Unknown;
}

TxSemanticKind ClassifyTxSemantic(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.vin.empty()) {
        return TxSemanticKind::Unknown;
    }

    const uint8_t input_asset = Consensus::GetInputAsset(tx, inputs);
    if (input_asset == 0) {
        return TxSemanticKind::Unknown;
    }

    // Dual-pool RES spends (POOL-T RES + POOL-F ORU fee inputs) are normal: GetInputAsset
    // returns RES whenever any RES prevout is present. Do NOT treat that as MixedInputDomain.
    if (Consensus::IsConvergenceTx(tx) && input_asset == Consensus::ASSET_ID_ORU) {
        return TxSemanticKind::OruConvergence;
    }
    if (Consensus::IsRESMintTx(tx, inputs)) {
        return TxSemanticKind::ResMint;
    }
    if (Consensus::IsRESSpendTx(tx, inputs)) {
        return TxHasResClaimOutput(tx) ? TxSemanticKind::ResClaim : TxSemanticKind::ResTransfer;
    }
    if (Consensus::IsORUTransferTx(tx, inputs)) {
        return TxSemanticKind::OruTransfer;
    }

    // Leftover: both asset domains present but no RFC-003 class matched (should be rare).
    bool has_oru = false;
    bool has_res = false;
    for (const CTxIn& in : tx.vin) {
        Coin coin;
        if (!inputs.GetCoin(in.prevout, coin)) return TxSemanticKind::Unknown;
        const AssetType a = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (a == ASSET_ORU) has_oru = true;
        else if (a == ASSET_RES) has_res = true;
        else return TxSemanticKind::Unknown;
    }
    if (has_oru && has_res) {
        return TxSemanticKind::MixedInputDomain;
    }
    return TxSemanticKind::Unknown;
}

TxSemanticKind ClassifyWalletTxSemantic(const CWallet& wallet, const CWalletTx& wtx)
{
    AssertLockHeld(wallet.cs_wallet);

    if (wtx.IsCoinBase()) {
        return TxSemanticKind::OruCoinbase;
    }

    if (wtx.tx->vin.empty()) {
        return TxSemanticKind::Unknown;
    }

    CCoinsView empty;
    CCoinsViewCache inputs(&empty);
    if (BuildWalletInputView(wallet, wtx, inputs)) {
        return ClassifyTxSemantic(*wtx.tx, inputs);
    }
    // Incoming txs: parent UTXOs are not in this wallet (and are spent on-chain).
    return ClassifyTxSemanticFromOutputs(*wtx.tx);
}

std::string TxSemanticCanonicalLabel(TxSemanticKind kind)
{
    switch (kind) {
    case TxSemanticKind::OruCoinbase: return "ORU Coinbase";
    case TxSemanticKind::OruTransfer: return "ORU Transfer";
    case TxSemanticKind::OruConvergence: return "ORU Convergence";
    case TxSemanticKind::ResTransfer: return "RES Transfer";
    case TxSemanticKind::ResMint: return "RES Mint";
    case TxSemanticKind::ResClaim: return "RES Claim";
    case TxSemanticKind::MixedInputDomain: return "Unexpected Mixed ORU/RES";
    case TxSemanticKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string TxSemanticRpcId(TxSemanticKind kind)
{
    switch (kind) {
    case TxSemanticKind::OruCoinbase: return "oru_coinbase";
    case TxSemanticKind::OruTransfer: return "oru_transfer";
    case TxSemanticKind::OruConvergence: return "oru_convergence";
    case TxSemanticKind::ResTransfer: return "res_transfer";
    case TxSemanticKind::ResMint: return "res_mint";
    case TxSemanticKind::ResClaim: return "res_claim";
    case TxSemanticKind::MixedInputDomain: return "mixed_input_domain";
    case TxSemanticKind::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace wallet
