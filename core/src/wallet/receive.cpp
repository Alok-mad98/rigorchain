// Copyright (c) 2021-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <stdexcept>
#include <util/time.h>
#include <wallet/receive.h>
#include <wallet/reuse_domain.h>
#include <wallet/transaction.h>
#include <wallet/wallet.h>

namespace wallet {
isminetype InputIsMine(const CWallet& wallet, const CTxIn& txin)
{
    AssertLockHeld(wallet.cs_wallet);
    const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
    if (prev && txin.prevout.n < prev->tx->vout.size()) {
        return wallet.IsMine(prev->tx->vout[txin.prevout.n]);
    }
    return ISMINE_NO;
}

bool AllInputsMine(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter)
{
    LOCK(wallet.cs_wallet);
    for (const CTxIn& txin : tx.vin) {
        if (!(InputIsMine(wallet, txin) & filter)) return false;
    }
    return true;
}

static CAmount OutputGetCreditUnlocked(const CWallet& wallet, const CTxOut& txout, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string("OutputGetCredit") + ": value out of range");
    // OP_CONVERGE / OP_RETURN never enter the UTXO set. Counting them as credit
    // would overstate spendable ORU if IsMine were ever true (watch-only import).
    if (!EntersUtxoSet(txout.scriptPubKey)) {
        return 0;
    }
    return ((wallet.IsMine(txout) & filter) ? txout.nValue : 0);
}

CAmount OutputGetCredit(const CWallet& wallet, const CTxOut& txout, const isminefilter& filter)
{
    LOCK(wallet.cs_wallet);
    return OutputGetCreditUnlocked(wallet, txout, filter);
}

CAmount TxGetCredit(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter)
{
    CAmount nCredit = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nCredit += OutputGetCredit(wallet, txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nCredit;
}

bool ScriptIsChange(const CWallet& wallet, const CScript& script)
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    AssertLockHeld(wallet.cs_wallet);
    if (!EntersUtxoSet(script)) {
        return false;
    }
    if (wallet.IsMine(script))
    {
        CTxDestination address;
        if (!ExtractDestination(script, address))
            return true;
        if (!wallet.FindAddressBookEntry(address)) {
            return true;
        }
    }
    return false;
}

bool OutputIsChange(const CWallet& wallet, const CTxOut& txout)
{
    return ScriptIsChange(wallet, txout.scriptPubKey);
}

CAmount OutputGetChange(const CWallet& wallet, const CTxOut& txout)
{
    AssertLockHeld(wallet.cs_wallet);
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error(std::string(__func__) + ": value out of range");
    return (OutputIsChange(wallet, txout) ? txout.nValue : 0);
}

CAmount TxGetChange(const CWallet& wallet, const CTransaction& tx)
{
    LOCK(wallet.cs_wallet);
    CAmount nChange = 0;
    for (const CTxOut& txout : tx.vout)
    {
        nChange += OutputGetChange(wallet, txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }
    return nChange;
}

static CAmount GetCachableAmount(const CWallet& wallet, const CWalletTx& wtx, CWalletTx::AmountType type, const isminefilter& filter)
{
    auto& amount = wtx.m_amounts[type];
    if (!amount.m_cached[filter]) {
        amount.Set(filter, type == CWalletTx::DEBIT ? wallet.GetDebit(*wtx.tx, filter) : TxGetCredit(wallet, *wtx.tx, filter));
        wtx.m_is_cache_empty = false;
    }
    return amount.m_value[filter];
}

CAmount CachedTxGetCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (wallet.IsTxImmatureCoinBase(wtx))
        return 0;

    CAmount credit = 0;
    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (get_amount_filter) {
        // GetBalance can assume transactions in mapWallet won't change
        credit += GetCachableAmount(wallet, wtx, CWalletTx::CREDIT, get_amount_filter);
    }
    return credit;
}

CAmount CachedTxGetDebit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    if (wtx.tx->vin.empty())
        return 0;

    CAmount debit = 0;
    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (get_amount_filter) {
        debit += GetCachableAmount(wallet, wtx, CWalletTx::DEBIT, get_amount_filter);
    }
    return debit;
}

CAmount CachedTxGetChange(const CWallet& wallet, const CWalletTx& wtx)
{
    if (wtx.fChangeCached)
        return wtx.nChangeCached;
    wtx.nChangeCached = TxGetChange(wallet, *wtx.tx);
    wtx.fChangeCached = true;
    return wtx.nChangeCached;
}

CAmount CachedTxGetImmatureCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    if (wallet.IsTxImmatureCoinBase(wtx) && wallet.IsTxInMainChain(wtx)) {
        return GetCachableAmount(wallet, wtx, CWalletTx::IMMATURE_CREDIT, filter);
    }

    return 0;
}

static CWalletTx::AmountType ImmatureCreditAmountType(AssetType asset)
{
    return asset == ASSET_RES ? CWalletTx::IMMATURE_CREDIT_RES : CWalletTx::IMMATURE_CREDIT_ORU;
}

static CWalletTx::AmountType AvailableCreditAmountType(AssetType asset)
{
    return asset == ASSET_RES ? CWalletTx::AVAILABLE_CREDIT_RES : CWalletTx::AVAILABLE_CREDIT_ORU;
}

static bool AllowAmountCache(const isminefilter& filter)
{
    // Match CachedTxGetAvailableCredit: never cache ISMINE_NO or ISMINE_ALL.
    return (filter & ISMINE_ALL) && (filter & ISMINE_ALL) != ISMINE_ALL;
}

CAmount CachedTxGetImmatureCreditForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    const bool immature_coinbase = wallet.IsTxImmatureCoinBase(wtx);
    const bool immature_res = (asset == ASSET_RES) && wallet.IsTxImmatureResMint(wtx);
    if ((!immature_coinbase && !immature_res) || !wallet.IsTxInMainChain(wtx)) {
        return 0;
    }

    const CWalletTx::AmountType amount_type = ImmatureCreditAmountType(asset);
    const bool allow_cache = AllowAmountCache(filter);
    if (allow_cache && wtx.m_amounts[amount_type].m_cached[filter]) {
        return wtx.m_amounts[amount_type].m_value[filter];
    }

    CAmount nCredit = 0;
    const uint256 hashTx = wtx.GetHash();
    for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i) {
        const CTxOut& txout = wtx.tx->vout[i];
        if (AssetTypeFromScriptPubKey(txout.scriptPubKey) != asset) continue;
        // Spent mint outputs must not remain in Pending while change is also counted.
        if (wallet.IsSpent(COutPoint(hashTx, i))) continue;
        nCredit += OutputGetCreditUnlocked(wallet, txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error(std::string(__func__) + ": value out of range");
    }

    if (allow_cache) {
        wtx.m_amounts[amount_type].Set(filter, nCredit);
        wtx.m_is_cache_empty = false;
    }
    return nCredit;
}

CAmount CachedTxGetCreditForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    // Immature coinbase: match legacy ORU behavior (history uses an unmatured special-case).
    // Immature body RES must still appear in credit_res for transaction history; spendability is
    // gated only via AvailableCredit / ImmatureCredit.
    if (wallet.IsTxImmatureCoinBase(wtx)) {
        return 0;
    }

    CAmount credit = 0;
    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (!get_amount_filter) {
        return 0;
    }

    for (const CTxOut& txout : wtx.tx->vout) {
        if (AssetTypeFromScriptPubKey(txout.scriptPubKey) != asset) continue;
        credit += OutputGetCredit(wallet, txout, get_amount_filter);
        if (!MoneyRange(credit)) {
            throw std::runtime_error(std::string(__func__) + ": value out of range");
        }
    }
    return credit;
}

CAmount CachedTxGetDebitForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    if (wtx.tx->vin.empty()) {
        return 0;
    }

    const isminefilter get_amount_filter{filter & ISMINE_ALL};
    if (!get_amount_filter) {
        return 0;
    }

    CAmount debit = 0;
    for (const CTxIn& txin : wtx.tx->vin) {
        const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
        if (!prev || txin.prevout.n >= prev->tx->vout.size()) continue;
        const CTxOut& prevout = prev->tx->vout[txin.prevout.n];
        if (AssetTypeFromScriptPubKey(prevout.scriptPubKey) != asset) continue;
        if (wallet.IsMine(prevout) & get_amount_filter) {
            debit += prevout.nValue;
            if (!MoneyRange(debit)) {
                throw std::runtime_error(std::string(__func__) + ": value out of range");
            }
        }
    }
    return debit;
}

CAmount CachedTxGetAvailableCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);

    // Avoid caching ismine for NO or ALL cases (could remove this check and simplify in the future).
    bool allow_cache = (filter & ISMINE_ALL) && (filter & ISMINE_ALL) != ISMINE_ALL;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (wallet.IsTxImmatureCoinBase(wtx))
        return 0;

    // Immature body RES mint: do not cache a blended total (ORU change may be spendable).
    const bool immature_res_mint = wallet.IsTxImmatureResMint(wtx);
    if (allow_cache && !immature_res_mint && wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].m_cached[filter]) {
        return wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].m_value[filter];
    }

    const bool avoid_reuse_active = wallet.IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE);
    const bool allow_used_addresses = (filter & ISMINE_USED) || !avoid_reuse_active;
    CAmount nCredit = 0;
    uint256 hashTx = wtx.GetHash();
    for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
        const CTxOut& txout = wtx.tx->vout[i];
        if (immature_res_mint && AssetTypeFromScriptPubKey(txout.scriptPubKey) == ASSET_RES) {
            continue;
        }
        if (!wallet.IsSpent(COutPoint(hashTx, i))
            && !OutputBlockedByOruReuse(wallet, txout.scriptPubKey, allow_used_addresses)) {
            nCredit += OutputGetCreditUnlocked(wallet, txout, filter);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    if (allow_cache && !immature_res_mint) {
        wtx.m_amounts[CWalletTx::AVAILABLE_CREDIT].Set(filter, nCredit);
        wtx.m_is_cache_empty = false;
    }

    return nCredit;
}

static CAmount CachedTxGetAvailableCreditForAsset(const CWallet& wallet, const CWalletTx& wtx, AssetType asset, const isminefilter& filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);

    // Immature amounts must not be written into the available-credit cache:
    // maturity flips on later blocks without MarkDirty().
    if (wallet.IsTxImmatureCoinBase(wtx))
        return 0;
    if (asset == ASSET_RES && wallet.IsTxImmatureResMint(wtx))
        return 0;

    const CWalletTx::AmountType amount_type = AvailableCreditAmountType(asset);
    const bool allow_cache = AllowAmountCache(filter);
    if (allow_cache && wtx.m_amounts[amount_type].m_cached[filter]) {
        return wtx.m_amounts[amount_type].m_value[filter];
    }

    const bool avoid_reuse_active = EffectiveAvoidReuseForAsset(
        wallet.IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE), asset);
    const bool allow_used_addresses = (filter & ISMINE_USED) || !avoid_reuse_active;
    CAmount nCredit = 0;
    uint256 hashTx = wtx.GetHash();
    for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
        const CTxOut& txout = wtx.tx->vout[i];
        if (AssetTypeFromScriptPubKey(txout.scriptPubKey) != asset) continue;
        if (!wallet.IsSpent(COutPoint(hashTx, i))
            && !OutputBlockedByOruReuse(wallet, txout.scriptPubKey, allow_used_addresses)) {
            nCredit += OutputGetCreditUnlocked(wallet, txout, filter);
            if (!MoneyRange(nCredit))
                throw std::runtime_error(std::string(__func__) + " : value out of range");
        }
    }

    if (allow_cache) {
        wtx.m_amounts[amount_type].Set(filter, nCredit);
        wtx.m_is_cache_empty = false;
    }
    return nCredit;
}

void CachedTxGetAmounts(const CWallet& wallet, const CWalletTx& wtx,
                  std::list<COutputEntry>& listReceived,
                  std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter,
                  bool include_change)
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();

    LOCK(wallet.cs_wallet);
    // Network fee is always paid in ORU (see CachedTxGetOruNetworkFee).
    CAmount nDebit = CachedTxGetDebit(wallet, wtx, filter);
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        nFee = CachedTxGetOruNetworkFee(wallet, wtx, filter);
    }

    // Sent/received.
    for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i)
    {
        const CTxOut& txout = wtx.tx->vout[i];
        isminetype fIsMine = wallet.IsMine(txout);
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0)
        {
            if (!include_change && OutputIsChange(wallet, txout))
                continue;
        }
        else if (!(fIsMine & filter))
            continue;

        // Destroyed outputs (OP_RETURN / OP_CONVERGE) never enter the UTXO set
        // and have no address. Do not call ExtractDestination — that logged
        // "Unknown transaction type" and stuffed CNoDestination into later scans.
        // When we funded a positive burn, list it as a send so details show B
        // separately from the ORU network fee. Zero-value markers are not funds.
        if (!EntersUtxoSet(txout.scriptPubKey)) {
            if (nDebit > 0 && txout.nValue > 0) {
                listSent.push_back({CNoDestination(), txout.nValue, (int)i});
            }
            continue;
        }

        // In either case, we need to get the destination address
        CTxDestination address;

        if (!ExtractDestination(txout.scriptPubKey, address) || !IsValidDestination(address))
        {
            wallet.WalletLogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                                    wtx.GetHash().ToString());
            address = CNoDestination();
        }

        COutputEntry output = {address, txout.nValue, (int)i};

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(output);

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine & filter)
            listReceived.push_back(output);
    }

}

bool CachedTxIsFromMe(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    return (CachedTxGetDebit(wallet, wtx, filter) > 0);
}

CAmount CachedTxGetOruNetworkFee(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter)
{
    AssertLockHeld(wallet.cs_wallet);
    if (!CachedTxIsFromMe(wallet, wtx, filter) || wtx.IsCoinBase()) {
        return 0;
    }
    CAmount oru_in{0};
    CAmount res_in{0};
    for (const auto& txin : wtx.tx->vin) {
        const CWalletTx* prev = wallet.GetWalletTx(txin.prevout.hash);
        if (!prev || txin.prevout.n >= prev->tx->vout.size()) continue;
        const CTxOut& po = prev->tx->vout[txin.prevout.n];
        if (wallet.IsMine(po) & filter) {
            if (AssetTypeFromScriptPubKey(po.scriptPubKey) == ASSET_ORU) {
                oru_in += po.nValue;
                if (!MoneyRange(oru_in)) {
                    throw std::runtime_error(std::string(__func__) + ": value out of range");
                }
            } else if (AssetTypeFromScriptPubKey(po.scriptPubKey) == ASSET_RES) {
                res_in += po.nValue;
                if (!MoneyRange(res_in)) {
                    throw std::runtime_error(std::string(__func__) + ": value out of range");
                }
            }
        }
    }
    // POOL-F: network fee is ORU-only; RES inputs never fund the fee.
    (void)res_in;
    const CAmount oru_out = wtx.tx->GetValueOut(ASSET_ORU);
    // Incomplete prevouts must not surface as a negative fee (a displayed credit).
    if (oru_in < oru_out) {
        return 0;
    }
    return oru_in - oru_out;
}

bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx, std::set<uint256>& trusted_parents)
{
    AssertLockHeld(wallet.cs_wallet);
    int nDepth = wallet.GetTxDepthInMainChain(wtx);
    if (nDepth >= 1) return true;
    if (nDepth < 0) return false;
    // using wtx's cached debit
    if (!wallet.m_spend_zero_conf_change || !CachedTxIsFromMe(wallet, wtx, ISMINE_ALL)) return false;

    // Don't trust unconfirmed transactions from us unless they are in the mempool.
    if (!wtx.InMempool()) return false;

    // Trusted if all inputs are from us and are in the mempool:
    for (const CTxIn& txin : wtx.tx->vin)
    {
        // Transactions not sent by us: not trusted
        const CWalletTx* parent = wallet.GetWalletTx(txin.prevout.hash);
        if (parent == nullptr) return false;
        const CTxOut& parentOut = parent->tx->vout[txin.prevout.n];
        // Check that this specific input being spent is trusted
        if (wallet.IsMine(parentOut) != ISMINE_SPENDABLE) return false;
        // If we've already trusted this parent, continue
        if (trusted_parents.count(parent->GetHash())) continue;
        // Recurse to check that the parent is also trusted
        if (!CachedTxIsTrusted(wallet, *parent, trusted_parents)) return false;
        trusted_parents.insert(parent->GetHash());
    }
    return true;
}

bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx)
{
    std::set<uint256> trusted_parents;
    LOCK(wallet.cs_wallet);
    return CachedTxIsTrusted(wallet, wtx, trusted_parents);
}

AssetBalanceDetail GetAssetBalance(const CWallet& wallet, AssetType asset, const int min_depth, bool avoid_reuse, const isminefilter credit_base)
{
    AssetBalanceDetail ret;
    const bool effective_avoid_reuse = EffectiveAvoidReuseForAsset(avoid_reuse, asset);
    const isminefilter reuse_filter = effective_avoid_reuse ? ISMINE_NO : ISMINE_USED;
    const isminefilter credit_filter = credit_base | reuse_filter;
    {
        LOCK(wallet.cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& entry : wallet.mapWallet)
        {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{CachedTxIsTrusted(wallet, wtx, trusted_parents)};
            const int tx_depth{wallet.GetTxDepthInMainChain(wtx)};
            const CAmount tx_credit_mine{CachedTxGetAvailableCreditForAsset(wallet, wtx, asset, credit_filter)};
            if (is_trusted && tx_depth >= min_depth) {
                ret.trusted += tx_credit_mine;
            }
            if (!is_trusted && tx_depth == 0 && wtx.InMempool()) {
                ret.pending += tx_credit_mine;
            }
        }
    }
    return ret;
}

CAmount GetAssetImmatureBalance(const CWallet& wallet, AssetType asset, const isminefilter& filter)
{
    CAmount total = 0;
    {
        LOCK(wallet.cs_wallet);
        for (const auto& entry : wallet.mapWallet) {
            total += CachedTxGetImmatureCreditForAsset(wallet, entry.second, asset, filter);
            if (!MoneyRange(total))
                throw std::runtime_error(std::string(__func__) + ": value out of range");
        }
    }
    return total;
}

static isminefilter AssetCreditFilter(AssetType asset, bool avoid_reuse, isminefilter credit_base)
{
    const bool effective_avoid_reuse = EffectiveAvoidReuseForAsset(avoid_reuse, asset);
    return credit_base | (effective_avoid_reuse ? ISMINE_NO : ISMINE_USED);
}

static void AccumulateTrustedPending(const CAmount credit, const bool is_trusted, const int tx_depth, const bool in_mempool,
                                     CAmount& trusted_min1, CAmount& trusted_min0, CAmount& pending)
{
    if (is_trusted && tx_depth >= 1) trusted_min1 += credit;
    if (is_trusted && tx_depth >= 0) trusted_min0 += credit;
    if (!is_trusted && tx_depth == 0 && in_mempool) pending += credit;
}

DualAssetWalletBalances GetDualAssetWalletBalances(const CWallet& wallet, const bool include_watch_only)
{
    DualAssetWalletBalances ret;
    const isminefilter oru_mine = AssetCreditFilter(ASSET_ORU, /*avoid_reuse=*/true, ISMINE_SPENDABLE);
    const isminefilter res_mine = AssetCreditFilter(ASSET_RES, /*avoid_reuse=*/true, ISMINE_SPENDABLE);
    const isminefilter oru_watch = AssetCreditFilter(ASSET_ORU, /*avoid_reuse=*/true, ISMINE_WATCH_ONLY);
    const isminefilter res_watch = AssetCreditFilter(ASSET_RES, /*avoid_reuse=*/true, ISMINE_WATCH_ONLY);

    const auto t0{SteadyClock::now()};
    size_t n_txs = 0;
    {
        LOCK(wallet.cs_wallet);
        n_txs = wallet.mapWallet.size();
        std::set<uint256> trusted_parents;
        for (const auto& entry : wallet.mapWallet) {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{CachedTxIsTrusted(wallet, wtx, trusted_parents)};
            const int tx_depth{wallet.GetTxDepthInMainChain(wtx)};
            const bool in_mempool{wtx.InMempool()};

            AccumulateTrustedPending(CachedTxGetAvailableCreditForAsset(wallet, wtx, ASSET_ORU, oru_mine),
                                     is_trusted, tx_depth, in_mempool,
                                     ret.oru_trusted_min1, ret.oru_trusted_min0, ret.oru_pending);
            AccumulateTrustedPending(CachedTxGetAvailableCreditForAsset(wallet, wtx, ASSET_RES, res_mine),
                                     is_trusted, tx_depth, in_mempool,
                                     ret.res_trusted_min1, ret.res_trusted_min0, ret.res_pending);
            ret.oru_immature += CachedTxGetImmatureCreditForAsset(wallet, wtx, ASSET_ORU, ISMINE_SPENDABLE);
            ret.res_immature += CachedTxGetImmatureCreditForAsset(wallet, wtx, ASSET_RES, ISMINE_SPENDABLE);
            if (!MoneyRange(ret.oru_immature) || !MoneyRange(ret.res_immature)) {
                throw std::runtime_error(std::string(__func__) + ": value out of range");
            }

            if (!include_watch_only) continue;

            AccumulateTrustedPending(CachedTxGetAvailableCreditForAsset(wallet, wtx, ASSET_ORU, oru_watch),
                                     is_trusted, tx_depth, in_mempool,
                                     ret.wo_oru_trusted_min1, ret.wo_oru_trusted_min0, ret.wo_oru_pending);
            AccumulateTrustedPending(CachedTxGetAvailableCreditForAsset(wallet, wtx, ASSET_RES, res_watch),
                                     is_trusted, tx_depth, in_mempool,
                                     ret.wo_res_trusted_min1, ret.wo_res_trusted_min0, ret.wo_res_pending);
            ret.wo_oru_immature += CachedTxGetImmatureCreditForAsset(wallet, wtx, ASSET_ORU, ISMINE_WATCH_ONLY);
            ret.wo_res_immature += CachedTxGetImmatureCreditForAsset(wallet, wtx, ASSET_RES, ISMINE_WATCH_ONLY);
            if (!MoneyRange(ret.wo_oru_immature) || !MoneyRange(ret.wo_res_immature)) {
                throw std::runtime_error(std::string(__func__) + ": value out of range");
            }
        }
    }
    const auto elapsed_ms = Ticks<std::chrono::milliseconds>(SteadyClock::now() - t0);
    if (elapsed_ms >= 50) {
        wallet.WalletLogPrintf("GetDualAssetWalletBalances %dms txs=%d watchonly=%s\n",
                               elapsed_ms, n_txs, include_watch_only ? "yes" : "no");
    }
    return ret;
}

Balance GetBalance(const CWallet& wallet, const int min_depth, bool avoid_reuse)
{
    Balance ret;
    isminefilter reuse_filter = avoid_reuse ? ISMINE_NO : ISMINE_USED;
    {
        LOCK(wallet.cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& entry : wallet.mapWallet)
        {
            const CWalletTx& wtx = entry.second;
            const bool is_trusted{CachedTxIsTrusted(wallet, wtx, trusted_parents)};
            const int tx_depth{wallet.GetTxDepthInMainChain(wtx)};
            const CAmount tx_credit_mine{CachedTxGetAvailableCredit(wallet, wtx, ISMINE_SPENDABLE | reuse_filter)};
            const CAmount tx_credit_watchonly{CachedTxGetAvailableCredit(wallet, wtx, ISMINE_WATCH_ONLY | reuse_filter)};
            if (is_trusted && tx_depth >= min_depth) {
                ret.m_mine_trusted += tx_credit_mine;
                ret.m_watchonly_trusted += tx_credit_watchonly;
            }
            if (!is_trusted && tx_depth == 0 && wtx.InMempool()) {
                ret.m_mine_untrusted_pending += tx_credit_mine;
                ret.m_watchonly_untrusted_pending += tx_credit_watchonly;
            }
            ret.m_mine_immature += CachedTxGetImmatureCredit(wallet, wtx, ISMINE_SPENDABLE);
            ret.m_watchonly_immature += CachedTxGetImmatureCredit(wallet, wtx, ISMINE_WATCH_ONLY);
        }
    }
    return ret;
}

std::map<CTxDestination, AddressNativeBalances> GetAddressBalancesByAsset(const CWallet& wallet)
{
    std::map<CTxDestination, AddressNativeBalances> balances;

    {
        LOCK(wallet.cs_wallet);
        std::set<uint256> trusted_parents;
        for (const auto& walletEntry : wallet.mapWallet)
        {
            const CWalletTx& wtx = walletEntry.second;

            if (!CachedTxIsTrusted(wallet, wtx, trusted_parents))
                continue;

            if (wallet.IsTxImmatureCoinBase(wtx))
                continue;

            int nDepth = wallet.GetTxDepthInMainChain(wtx);
            if (nDepth < (CachedTxIsFromMe(wallet, wtx, ISMINE_ALL) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
                const auto& output = wtx.tx->vout[i];
                if (!EntersUtxoSet(output.scriptPubKey)) {
                    continue;
                }
                if (AssetTypeFromScriptPubKey(output.scriptPubKey) == ASSET_RES && wallet.IsTxImmatureResMint(wtx)) {
                    continue;
                }
                CTxDestination addr;
                if (!wallet.IsMine(output))
                    continue;
                if(!ExtractDestination(output.scriptPubKey, addr) || !IsValidDestination(addr))
                    continue;

                const CAmount n = wallet.IsSpent(COutPoint(walletEntry.first, i)) ? 0 : output.nValue;
                const AssetType asset = AssetTypeFromScriptPubKey(output.scriptPubKey);
                if (asset == ASSET_RES) {
                    balances[addr].res += n;
                } else if (asset == ASSET_ORU) {
                    balances[addr].oru += n;
                }
            }
        }
    }

    return balances;
}

std::map<CTxDestination, CAmount> GetAddressBalances(const CWallet& wallet)
{
    std::map<CTxDestination, CAmount> balances;
    for (const auto& [addr, by_asset] : GetAddressBalancesByAsset(wallet)) {
        balances[addr] = by_asset.oru;
    }
    return balances;
}

std::set< std::set<CTxDestination> > GetAddressGroupings(const CWallet& wallet)
{
    AssertLockHeld(wallet.cs_wallet);
    std::set< std::set<CTxDestination> > groupings;
    std::set<CTxDestination> grouping;

    for (const auto& walletEntry : wallet.mapWallet)
    {
        const CWalletTx& wtx = walletEntry.second;

        if (wtx.tx->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            for (const CTxIn& txin : wtx.tx->vin)
            {
                CTxDestination address;
                if(!InputIsMine(wallet, txin)) /* If this input isn't mine, ignore it */
                    continue;
                if(!ExtractDestination(wallet.mapWallet.at(txin.prevout.hash).tx->vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               for (const CTxOut& txout : wtx.tx->vout)
                   if (OutputIsChange(wallet, txout))
                   {
                       CTxDestination txoutAddr;
                       if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                           continue;
                       grouping.insert(txoutAddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (const auto& txout : wtx.tx->vout)
            if (wallet.IsMine(txout))
            {
                CTxDestination address;
                if(!ExtractDestination(txout.scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    std::set< std::set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    std::map< CTxDestination, std::set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    for (const std::set<CTxDestination>& _grouping : groupings)
    {
        // make a set of all the groups hit by this new group
        std::set< std::set<CTxDestination>* > hits;
        std::map< CTxDestination, std::set<CTxDestination>* >::iterator it;
        for (const CTxDestination& address : _grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        std::set<CTxDestination>* merged = new std::set<CTxDestination>(_grouping);
        for (std::set<CTxDestination>* hit : hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        for (const CTxDestination& element : *merged)
            setmap[element] = merged;
    }

    std::set< std::set<CTxDestination> > ret;
    for (const std::set<CTxDestination>* uniqueGrouping : uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}
} // namespace wallet
