// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionrecord.h>

#include <chain.h>
#include <consensus/convergenceres.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <wallet/tx_semantic.h>
#include <wallet/types.h>

#include <stdint.h>

#include <QDateTime>

using wallet::ISMINE_SPENDABLE;
using wallet::ISMINE_WATCH_ONLY;
using wallet::isminetype;

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction()
{
    // There are currently no cases where we hide transactions, but
    // we may want to use this in the future for things like RBF.
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const interfaces::WalletTx& wtx)
{
    QList<TransactionRecord> parts;
    int64_t nTime = wtx.time;
    const wallet::TxSemanticKind tx_semantic = wtx.semantic;
    uint256 hash = wtx.tx->GetHash();
    std::map<std::string, std::string> mapValue = wtx.value_map;

    // Body RES mint: always list mineable RES outputs at face value (maturity must not zero the
    // history Amount column; details already show Credit via getCredit(txout)).
    if (tx_semantic == wallet::TxSemanticKind::ResMint) {
        for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i) {
            if (!wtx.txout_is_mine[i]) continue;
            if (AssetTypeFromScriptPubKey(wtx.tx->vout[i].scriptPubKey) != ASSET_RES) continue;
            TransactionRecord sub(hash, nTime);
            sub.idx = static_cast<int>(i);
            sub.credit = wtx.tx->vout[i].nValue;
            sub.semantic = tx_semantic;
            sub.amount_asset = ASSET_RES;
            sub.involvesWatchAddress = wtx.txout_is_mine[i] & ISMINE_WATCH_ONLY;
            if (wtx.txout_address_is_mine[i]) {
                sub.type = TransactionRecord::RecvWithAddress;
                sub.address = EncodeDestination(wtx.txout_address[i]);
            } else {
                sub.type = TransactionRecord::RecvFromOther;
                sub.address = mapValue["from"];
            }
            parts.append(sub);
        }
        if (!parts.empty()) return parts;
    }

    // Own createconvergence spends ORU and usually returns change → credit_oru > 0.
    // That must not take the generic "credit" path (which would list change as the
    // Convergence amount). Route funded convergences through the debit path so the
    // UI amount is the CVG2 locked value (GetConvergenceAmount / vout nValue).
    const bool own_oru_convergence =
        tx_semantic == wallet::TxSemanticKind::OruConvergence && wtx.debit_oru > 0;

    // Own ORU send with change: credit_oru > 0 for the change output. Bitcoin Core
    // lists the external payee (debit path), not the change as a "receive".
    const bool own_oru_transfer =
        tx_semantic == wallet::TxSemanticKind::OruTransfer && wtx.debit_oru > 0;

    // Dual-pool RES send (POOL-T RES + POOL-F ORU fee) usually returns ORU change →
    // credit_oru > 0. Do not list that change as a receive; show the RES payment instead.
    const bool own_res_transfer =
        (tx_semantic == wallet::TxSemanticKind::ResTransfer ||
         tx_semantic == wallet::TxSemanticKind::ResClaim) &&
        wtx.debit_res > 0;

    if (!own_oru_convergence && !own_oru_transfer && !own_res_transfer &&
        (wtx.is_coinbase || wtx.credit_oru > 0 || wtx.credit_res > 0))
    {
        //
        // Credit
        //
        for(unsigned int i = 0; i < wtx.tx->vout.size(); i++)
        {
            const CTxOut& txout = wtx.tx->vout[i];
            isminetype mine = wtx.txout_is_mine[i];
            if(mine)
            {
                TransactionRecord sub(hash, nTime);
                sub.idx = i; // vout index
                sub.credit = txout.nValue;
                sub.semantic = tx_semantic;
                sub.amount_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (wtx.txout_address_is_mine[i])
                {
                    // Received by Rigor Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = EncodeDestination(wtx.txout_address[i]);
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.is_coinbase)
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool involvesWatchAddress = false;
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txin_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txout_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            std::string address;
            for (unsigned int oi = 0; oi < wtx.tx->vout.size(); ++oi) {
                if (Consensus::IsConvergenceOutputScript(wtx.tx->vout[oi].scriptPubKey)) continue;
                if (std::get_if<CNoDestination>(&wtx.txout_address[oi])) continue;
                if (!address.empty()) address += ", ";
                address += EncodeDestination(wtx.txout_address[oi]);
            }

            CAmount nChange = wtx.change;
            AssetType self_asset = ASSET_ORU;
            for (unsigned int oi = 0; oi < wtx.tx->vout.size(); ++oi) {
                if (Consensus::IsConvergenceOutputScript(wtx.tx->vout[oi].scriptPubKey)) continue;
                if (!wtx.txout_is_mine[oi]) continue;
                self_asset = AssetTypeFromScriptPubKey(wtx.tx->vout[oi].scriptPubKey);
                break;
            }
            const CAmount self_debit = self_asset == ASSET_RES ? wtx.debit_res : wtx.debit_oru;
            const CAmount self_credit = self_asset == ASSET_RES ? wtx.credit_res : wtx.credit_oru;
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, address,
                -(self_debit - nChange), self_credit - nChange, self_asset));
            parts.last().semantic = tx_semantic;
            parts.last().involvesWatchAddress = involvesWatchAddress;   // maybe pass to TransactionRecord as constructor argument
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            CAmount nTxFee = wtx.fee_oru;

            for (unsigned int nOut = 0; nOut < wtx.tx->vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.tx->vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = nOut;
                sub.involvesWatchAddress = involvesWatchAddress;

                if(wtx.txout_is_mine[nOut])
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                const bool is_cvg = Consensus::IsConvergenceOutputScript(txout.scriptPubKey);
                // RESO / OP_RETURN companions are zero-value metadata — not a second "amount" row.
                if (!is_cvg && txout.nValue == 0) {
                    continue;
                }

                if (is_cvg)
                {
                    sub.type = TransactionRecord::SendToConverge;
                    sub.address.clear();
                }
                else if (!std::get_if<CNoDestination>(&wtx.txout_address[nOut]))
                {
                    // Sent to Rigor Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = EncodeDestination(wtx.txout_address[nOut]);
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                CAmount nValue = txout.nValue;
                /* Fold ORU network fee into the first ORU payee row only (POOL-F).
                 * Never add ORU fee sats onto a RES transfer amount (POOL-T). */
                const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                if (nTxFee > 0 && !is_cvg && out_asset == ASSET_ORU)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;
                sub.amount_asset = out_asset;
                sub.semantic = tx_semantic;

                parts.append(sub);
            }
            // Convergence-only sends: leave fee out of the CVG2 row so Amount = locked ORU.
            // (Wallet balances already account for fee via debit/credit; UI prioritizes locked amount.)
            (void)nTxFee;
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            AssetType mixed_asset = ASSET_ORU;
            for (const CTxOut& txout : wtx.tx->vout) {
                if (txout.nValue > 0) {
                    mixed_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                    break;
                }
            }
            const CAmount mixed_net = mixed_asset == ASSET_RES
                ? (wtx.credit_res - wtx.debit_res) : (wtx.credit_oru - wtx.debit_oru);
            parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", mixed_net, 0, mixed_asset));
            parts.last().semantic = tx_semantic;
            parts.last().involvesWatchAddress = involvesWatchAddress;
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const interfaces::WalletTxStatus& wtx, const uint256& block_hash, int numBlocks, int64_t block_time)
{
    // Determine transaction status

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        wtx.block_height,
        wtx.is_coinbase ? 1 : 0,
        wtx.time_received,
        idx);
    status.countsForBalance = wtx.is_trusted && !(wtx.blocks_to_maturity > 0);
    status.depth = wtx.depth_in_main_chain;
    status.m_cur_block_hash = block_hash;

    // For generated transactions, determine maturity
    if (type == TransactionRecord::Generated) {
        if (wtx.blocks_to_maturity > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.is_in_main_chain)
            {
                status.matures_in = wtx.blocks_to_maturity;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.is_abandoned)
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    status.needsUpdate = false;
}

bool TransactionRecord::statusUpdateNeeded(const uint256& block_hash) const
{
    assert(!block_hash.IsNull());
    return status.m_cur_block_hash != block_hash || status.needsUpdate;
}

QString TransactionRecord::getTxHash() const
{
    return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}
