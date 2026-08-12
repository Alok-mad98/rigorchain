// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/fee_domain.h>

#include <core_io.h>
#include <rpc/util.h>
#include <script/standard.h>
#include <util/check.h>
#include <util/translation.h>
#include <wallet/coincontrol.h>
#include <wallet/receive.h>
#include <wallet/spend.h>
#include <wallet/wallet.h>

namespace wallet {

CAmount OruNetworkFeeForSizeBytes(const CFeeRate& feerate, int size_bytes)
{
    if (size_bytes <= 0) return 0;
    return feerate.GetFee(size_bytes);
}

bool WalletHasSpendableOruForFee(const CWallet& wallet, const CCoinControl* coin_control)
{
    LOCK(wallet.cs_wallet);
    CoinFilterParams params;
    params.m_asset_type = ASSET_ORU;
    CoinsResult coins = AvailableCoins(wallet, coin_control, std::nullopt, params);
    return coins.GetTotalAmount() > 0;
}

std::optional<bilingual_str> CheckResTransferFeeSemantics(const std::vector<CRecipient>& vecSend)
{
    for (const auto& recipient : vecSend) {
        if (recipient.fSubtractFeeFromAmount) {
            return _("Cannot subtract the ORU network fee from RES outputs; pay the fee from ORU inputs.");
        }
    }
    return std::nullopt;
}

void AssertWalletTxNetworkFeeOruOnly(const CTransaction& tx, const CWallet& wallet, CAmount reported_fee_oru)
{
    CAmount oru_in{0};
    CAmount oru_out{0};
    CAmount res_in{0};
    CAmount res_out{0};
    for (const CTxIn& txin : tx.vin) {
        const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
        if (!prev || txin.prevout.n >= prev->tx->vout.size()) continue;
        const CTxOut& po = prev->tx->vout[txin.prevout.n];
        if (AssetTypeFromScriptPubKey(po.scriptPubKey) == ASSET_ORU) {
            oru_in += po.nValue;
        } else if (AssetTypeFromScriptPubKey(po.scriptPubKey) == ASSET_RES) {
            res_in += po.nValue;
        }
    }
    for (const CTxOut& txout : tx.vout) {
        if (AssetTypeFromScriptPubKey(txout.scriptPubKey) == ASSET_ORU) {
            oru_out += txout.nValue;
        } else if (AssetTypeFromScriptPubKey(txout.scriptPubKey) == ASSET_RES) {
            res_out += txout.nValue;
        }
    }
    if (res_in > 0) {
        // POOL-T: RES inputs cover RES outputs only (no RES-denominated network fee).
        Assert(res_in >= res_out);
        Assert(res_in == res_out);
    }
    if (oru_in > 0) {
        const CAmount implied_fee = oru_in - oru_out;
        Assert(implied_fee >= 0);
        Assert(reported_fee_oru == implied_fee);
    } else if (res_in > 0) {
        // RES transfer without ORU fee inputs is forbidden at construction time.
        Assert(false);
    }
}

CAmount WalletTxOruNetworkFee(const CWallet& wallet, const CWalletTx& wtx, isminefilter filter)
{
    LOCK(wallet.cs_wallet);
    return CachedTxGetOruNetworkFee(wallet, wtx, filter);
}

std::optional<bilingual_str> CheckSubtractFeeFromResOutputs(const CMutableTransaction& tx,
    const std::set<int>& subtract_vout_indices)
{
    for (const int idx : subtract_vout_indices) {
        if (idx < 0 || static_cast<size_t>(idx) >= tx.vout.size()) continue;
        if (AssetTypeFromScriptPubKey(tx.vout.at(static_cast<size_t>(idx)).scriptPubKey) == ASSET_RES) {
            return _("Cannot subtract the ORU network fee from RES outputs; pay the fee from ORU inputs.");
        }
    }
    return std::nullopt;
}

void PushRpcOruNetworkFeeFields(UniValue& obj, const std::string& amount_key, CAmount fee_oru)
{
    obj.pushKV(amount_key, ValueFromAmount(fee_oru));
    const std::string oru_suffix_key = amount_key + "_oru";
    obj.pushKV(oru_suffix_key, ValueFromAmount(fee_oru));
    if (amount_key == "fee" || amount_key == "origfee") {
        obj.pushKV("fee_asset", "ORU");
    }
}

bilingual_str NormalizeWalletSpendError(const bilingual_str& err)
{
    return err;
}

} // namespace wallet
