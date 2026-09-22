// Copyright (c) 2021-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RECEIVE_H
#define BITCOIN_WALLET_RECEIVE_H

#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <wallet/transaction.h>
#include <wallet/types.h>
#include <wallet/wallet.h>

namespace wallet {
isminetype InputIsMine(const CWallet& wallet, const CTxIn& txin) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

/** Returns whether all of the inputs match the filter */
bool AllInputsMine(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter);

CAmount OutputGetCredit(const CWallet& wallet, const CTxOut& txout, const isminefilter& filter);
CAmount TxGetCredit(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter);

bool ScriptIsChange(const CWallet& wallet, const CScript& script) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
bool OutputIsChange(const CWallet& wallet, const CTxOut& txout) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount OutputGetChange(const CWallet& wallet, const CTxOut& txout) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount TxGetChange(const CWallet& wallet, const CTransaction& tx);

CAmount CachedTxGetCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
//! filter decides which addresses will count towards the debit
CAmount CachedTxGetDebit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter);
CAmount CachedTxGetChange(const CWallet& wallet, const CWalletTx& wtx);
CAmount CachedTxGetImmatureCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount CachedTxGetImmatureCreditForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount CachedTxGetCreditForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount CachedTxGetDebitForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount CachedTxGetAvailableCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter = ISMINE_SPENDABLE)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};
void CachedTxGetAmounts(const CWallet& wallet, const CWalletTx& wtx,
                        std::list<COutputEntry>& listReceived,
                        std::list<COutputEntry>& listSent,
                        CAmount& nFee, const isminefilter& filter,
                        bool include_change);
bool CachedTxIsFromMe(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter);
/** Network fee paid in ORU (sum of our ORU inputs minus total ORU outputs). Never negative. */
CAmount CachedTxGetOruNetworkFee(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx, std::set<uint256>& trusted_parents) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx);

struct Balance {
    CAmount m_mine_trusted{0};           //!< Trusted, at depth=GetBalance.min_depth or more
    CAmount m_mine_untrusted_pending{0}; //!< Untrusted, but in mempool (pending)
    CAmount m_mine_immature{0};          //!< Immature coinbases in the main chain
    CAmount m_watchonly_trusted{0};
    CAmount m_watchonly_untrusted_pending{0};
    CAmount m_watchonly_immature{0};
};
Balance GetBalance(const CWallet& wallet, int min_depth = 0, bool avoid_reuse = true);

/** Trusted (spendable at min_depth) and mempool-pending credit for a single native asset. */
struct AssetBalanceDetail {
    CAmount trusted{0};
    CAmount pending{0};
};
/** @param avoid_reuse ORU-only reuse filter; ignored for RES (always fully visible). */
AssetBalanceDetail GetAssetBalance(const CWallet& wallet, AssetType asset, int min_depth = 0, bool avoid_reuse = true, isminefilter credit_base = ISMINE_SPENDABLE);
/** Immature coinbase credit for one asset (e.g. locked RES). */
CAmount GetAssetImmatureBalance(const CWallet& wallet, AssetType asset, const isminefilter& filter = ISMINE_SPENDABLE);

/** One mapWallet walk covering the six GetAssetBalance/GetAssetImmatureBalance
 *  queries the GUI performs after every block. Watch-only fields are left 0
 *  when include_watch_only is false. */
struct DualAssetWalletBalances {
    CAmount oru_trusted_min1{0};
    CAmount oru_trusted_min0{0};
    CAmount oru_pending{0};
    CAmount oru_immature{0};
    CAmount res_trusted_min1{0};
    CAmount res_trusted_min0{0};
    CAmount res_pending{0};
    CAmount res_immature{0};
    CAmount wo_oru_trusted_min1{0};
    CAmount wo_oru_trusted_min0{0};
    CAmount wo_oru_pending{0};
    CAmount wo_oru_immature{0};
    CAmount wo_res_trusted_min1{0};
    CAmount wo_res_trusted_min0{0};
    CAmount wo_res_pending{0};
    CAmount wo_res_immature{0};
};
DualAssetWalletBalances GetDualAssetWalletBalances(const CWallet& wallet, bool include_watch_only = false);

struct AddressNativeBalances {
    CAmount oru{0};
    CAmount res{0};
};
std::map<CTxDestination, AddressNativeBalances> GetAddressBalancesByAsset(const CWallet& wallet);
//! @deprecated Mixed-domain sum; prefer GetAddressBalancesByAsset.
std::map<CTxDestination, CAmount> GetAddressBalances(const CWallet& wallet);
std::set<std::set<CTxDestination>> GetAddressGroupings(const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
} // namespace wallet

#endif // BITCOIN_WALLET_RECEIVE_H
