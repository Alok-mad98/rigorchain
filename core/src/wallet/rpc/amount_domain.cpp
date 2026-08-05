// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpc/amount_domain.h>

#include <core_io.h>
#include <rpc/util.h>
#include <wallet/receive.h>

namespace wallet {

WalletTxDomainNet WalletTxDomainNetAmounts(const CWallet& wallet, const CWalletTx& wtx, isminefilter filter)
{
    AssertLockHeld(wallet.cs_wallet);
    WalletTxDomainNet net;
    net.oru = CachedTxGetCreditForAsset(wallet, wtx, ASSET_ORU, filter)
          - CachedTxGetDebitForAsset(wallet, wtx, ASSET_ORU, filter);
    net.res = CachedTxGetCreditForAsset(wallet, wtx, ASSET_RES, filter)
          - CachedTxGetDebitForAsset(wallet, wtx, ASSET_RES, filter);
    return net;
}

void PushWalletTxDomainAmountFields(UniValue& entry, const WalletTxDomainNet& net)
{
    entry.pushKV("amount_oru", ValueFromAmountAsset(net.oru, ASSET_ORU));
    entry.pushKV("amount_res", ValueFromAmountAsset(net.res, ASSET_RES));
    // Legacy field: ORU-domain net only (excludes RES movements; fee is separate).
    entry.pushKV("amount", ValueFromAmountAsset(net.oru, ASSET_ORU));
}

void PushReceivedTallyAmountFields(UniValue& obj, CAmount oru, CAmount res)
{
    obj.pushKV("amount_oru", ValueFromAmountAsset(oru, ASSET_ORU));
    obj.pushKV("amount_res", ValueFromAmountAsset(res, ASSET_RES));
    obj.pushKV("amount", ValueFromAmountAsset(oru, ASSET_ORU));
}

void PushSendCategoryOruFeeFields(UniValue& entry, CAmount fee_oru)
{
    if (fee_oru == 0) return;
    entry.pushKV("fee", ValueFromAmount(-fee_oru));
    entry.pushKV("fee_oru", ValueFromAmount(fee_oru));
}

} // namespace wallet
