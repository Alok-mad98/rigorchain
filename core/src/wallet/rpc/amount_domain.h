// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPC_AMOUNT_DOMAIN_H
#define BITCOIN_WALLET_RPC_AMOUNT_DOMAIN_H

#include <consensus/amount.h>
#include <script/standard.h>
#include <univalue.h>
#include <wallet/transaction.h>
#include <wallet/wallet.h>

namespace wallet {

/** Per-native-asset net (credit minus debit) for one wallet transaction. */
struct WalletTxDomainNet {
    CAmount oru{0};
    CAmount res{0};
};

WalletTxDomainNet WalletTxDomainNetAmounts(const CWallet& wallet, const CWalletTx& wtx, isminefilter filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

/** Top-level gettransaction amount fields (domain-split; legacy `amount` is ORU-only net). */
void PushWalletTxDomainAmountFields(UniValue& entry, const WalletTxDomainNet& net);

/** listreceived* / listaddressgroupings received totals (never ORU+RES in one field). */
void PushReceivedTallyAmountFields(UniValue& obj, CAmount oru, CAmount res);

/** Send-category list row: ORU network fee (legacy `fee` stays negative ORU). */
void PushSendCategoryOruFeeFields(UniValue& entry, CAmount fee_oru);

} // namespace wallet

#endif // BITCOIN_WALLET_RPC_AMOUNT_DOMAIN_H
