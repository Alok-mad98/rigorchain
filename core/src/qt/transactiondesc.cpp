// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifdef HAVE_CONFIG_H
#include <config/rigor-config.h>
#endif

#include <qt/transactiondesc.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/paymentserver.h>
#include <qt/transactionrecord.h>

#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus_rigor/fee.h>
#include <consensus/convergenceres.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/tx_semantic.h>
#include <wallet/types.h>

#include <stdint.h>
#include <set>
#include <string>

#include <QLatin1String>

using wallet::ISMINE_ALL;
using wallet::ISMINE_SPENDABLE;
using wallet::ISMINE_WATCH_ONLY;
using wallet::isminetype;

QString TransactionDesc::FormatTxStatus(const interfaces::WalletTxStatus& status, bool inMempool)
{
    int depth = status.depth_in_main_chain;
    if (depth < 0) {
        /*: Text explaining the current status of a transaction, shown in the
            status field of the details window for this transaction. This status
            represents an unconfirmed transaction that conflicts with a confirmed
            transaction. */
        return tr("conflicted with a transaction with %1 confirmations").arg(-depth);
    } else if (depth == 0) {
        QString s;
        if (inMempool) {
            /*: Text explaining the current status of a transaction, shown in the
                status field of the details window for this transaction. This status
                represents an unconfirmed transaction that is in the memory pool. */
            s = tr("0/unconfirmed, in memory pool");
        } else {
            /*: Text explaining the current status of a transaction, shown in the
                status field of the details window for this transaction. This status
                represents an unconfirmed transaction that is not in the memory pool. */
            s = tr("0/unconfirmed, not in memory pool");
        }
        if (status.is_abandoned) {
            /*: Text explaining the current status of a transaction, shown in the
                status field of the details window for this transaction. This
                status represents an abandoned transaction. */
            s += QLatin1String(", ") + tr("abandoned");
        }
        return s;
    } else if (depth < 6) {
        /*: Text explaining the current status of a transaction, shown in the
            status field of the details window for this transaction. This
            status represents a transaction confirmed in at least one block,
            but less than 6 blocks. */
        return tr("%1/unconfirmed").arg(depth);
    } else {
        /*: Text explaining the current status of a transaction, shown in the
            status field of the details window for this transaction. This status
            represents a transaction confirmed in 6 or more blocks. */
        return tr("%1 confirmations").arg(depth);
    }
}

// Takes an encoded PaymentRequest as a string and tries to find the Common Name of the X.509 certificate
// used to sign the PaymentRequest.
bool GetPaymentRequestMerchant(const std::string& pr, QString& merchant)
{
    // Search for the supported pki type strings
    if (pr.find(std::string({0x12, 0x0b}) + "x509+sha256") != std::string::npos || pr.find(std::string({0x12, 0x09}) + "x509+sha1") != std::string::npos) {
        // We want the common name of the Subject of the cert. This should be the second occurrence
        // of the bytes 0x0603550403. The first occurrence of those is the common name of the issuer.
        // After those bytes will be either 0x13 or 0x0C, then length, then either the ascii or utf8
        // string with the common name which is the merchant name
        size_t cn_pos = pr.find({0x06, 0x03, 0x55, 0x04, 0x03});
        if (cn_pos != std::string::npos) {
            cn_pos = pr.find({0x06, 0x03, 0x55, 0x04, 0x03}, cn_pos + 5);
            if (cn_pos != std::string::npos) {
                cn_pos += 5;
                if (pr[cn_pos] == 0x13 || pr[cn_pos] == 0x0c) {
                    cn_pos++; // Consume the type
                    int str_len = pr[cn_pos];
                    cn_pos++; // Consume the string length
                    merchant = QString::fromUtf8(pr.data() + cn_pos, str_len);
                    return true;
                }
            }
        }
    }
    return false;
}

QString TransactionDesc::toHTML(interfaces::Node& node, interfaces::Wallet& wallet, TransactionRecord* rec, BitcoinUnit unit)
{
    int numBlocks;
    interfaces::WalletTxStatus status;
    interfaces::WalletOrderForm orderForm;
    bool inMempool;
    interfaces::WalletTx wtx = wallet.getWalletTxDetails(rec->hash, status, orderForm, inMempool, numBlocks);

    QString strHTML;

    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t nTime = wtx.time;
    const CAmount net_oru = wtx.credit_oru - wtx.debit_oru;
    const CAmount net_res = wtx.credit_res - wtx.debit_res;

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(status, inMempool);
    strHTML += "<br>";
    // Prefer Bitcoin-style role for coinbase; otherwise RFC-003 semantic label.
    strHTML += "<b>" + tr("Type") + ":</b> ";
    if (wtx.is_coinbase || rec->type == TransactionRecord::Generated) {
        strHTML += tr("Mined");
    } else if (rec->type == TransactionRecord::SendToAddress ||
               rec->type == TransactionRecord::SendToOther) {
        strHTML += tr("Sent to");
    } else if (rec->type == TransactionRecord::RecvWithAddress) {
        strHTML += tr("Received with");
    } else if (rec->type == TransactionRecord::RecvFromOther) {
        strHTML += tr("Received from");
    } else if (rec->type == TransactionRecord::SendToSelf) {
        strHTML += tr("Payment to yourself");
    } else {
        strHTML += QString::fromStdString(wallet::TxSemanticCanonicalLabel(wtx.semantic));
    }
    strHTML += "<br>";

    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

    //
    // From
    //
    if (wtx.is_coinbase)
    {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Coinbase") + "<br>";
    }
    else if (wtx.value_map.count("from") && !wtx.value_map["from"].empty())
    {
        // Online transaction
        strHTML += "<b>" + tr("From") + ":</b> " + GUIUtil::HtmlEscape(wtx.value_map["from"]) + "<br>";
    }
    else
    {
        // Offline transaction
        if (net_oru > 0 || net_res > 0)
        {
            // Credit
            CTxDestination address = DecodeDestination(rec->address);
            if (IsValidDestination(address)) {
                std::string name;
                isminetype ismine;
                if (wallet.getAddress(address, &name, &ismine, /* purpose= */ nullptr))
                {
                    strHTML += "<b>" + tr("From") + ":</b> " + tr("unknown") + "<br>";
                    strHTML += "<b>" + tr("To") + ":</b> ";
                    strHTML += GUIUtil::HtmlEscape(rec->address);
                    QString addressOwned = ismine == ISMINE_SPENDABLE ? tr("own address") : tr("watch-only");
                    if (!name.empty())
                        strHTML += " (" + addressOwned + ", " + tr("label") + ": " + GUIUtil::HtmlEscape(name) + ")";
                    else
                        strHTML += " (" + addressOwned + ")";
                    strHTML += "<br>";
                }
            }
        }
    }

    //
    // To
    //
    if (wtx.value_map.count("to") && !wtx.value_map["to"].empty())
    {
        // Online transaction
        std::string strAddress = wtx.value_map["to"];
        strHTML += "<b>" + tr("To") + ":</b> ";
        CTxDestination dest = DecodeDestination(strAddress);
        std::string name;
        if (wallet.getAddress(
                dest, &name, /* is_mine= */ nullptr, /* purpose= */ nullptr) && !name.empty())
            strHTML += GUIUtil::HtmlEscape(name) + " ";
        strHTML += GUIUtil::HtmlEscape(strAddress) + "<br>";
    }

    //
    // Amount
    //
    if (wtx.is_coinbase && wtx.credit_oru == 0 && wtx.credit_res == 0)
    {
        //
        // Coinbase
        //
        CAmount nUnmatured = 0;
        for (const CTxOut& txout : wtx.tx->vout)
            nUnmatured += wallet.getCredit(txout, ISMINE_ALL);
        AssetType coinbase_asset = rec->amount_asset;
        for (const CTxOut& txout : wtx.tx->vout) {
            if (txout.nValue > 0) {
                coinbase_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                break;
            }
        }
        strHTML += "<b>" + tr("Credit") + ":</b> ";
        if (status.is_in_main_chain)
            strHTML += BitcoinUnits::formatAmountHtmlForAsset(coinbase_asset, nUnmatured, unit) +
                " (" + tr("matures in %n more block(s)", "", status.blocks_to_maturity) + ")";
        else
            strHTML += "(" + tr("not accepted") + ")";
        strHTML += "<br>";
    }
    else if (net_oru > 0 || net_res > 0)
    {
        //
        // Credit
        //
        if (net_oru > 0) {
            strHTML += "<b>" + tr("Credit") + " (ORU):</b> " +
                BitcoinUnits::formatAmountHtmlForAsset(ASSET_ORU, net_oru, unit) + "<br>";
        }
        if (net_res > 0) {
            strHTML += "<b>" + tr("Credit") + " (RES):</b> " +
                BitcoinUnits::formatAmountHtmlForAsset(ASSET_RES, net_res, unit) + "<br>";
        }
    }
    else
    {
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txin_is_mine)
        {
            if(fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txout_is_mine)
        {
            if(fAllToMe > mine) fAllToMe = mine;
        }

        if (fAllFromMe)
        {
            if(fAllFromMe & ISMINE_WATCH_ONLY)
                strHTML += "<b>" + tr("From") + ":</b> " + tr("watch-only") + "<br>";

            //
            // Debit
            //
            auto mine = wtx.txout_is_mine.begin();
            for (const CTxOut& txout : wtx.tx->vout)
            {
                // Ignore change
                isminetype toSelf = *(mine++);
                if ((toSelf == ISMINE_SPENDABLE) && (fAllFromMe == ISMINE_SPENDABLE))
                    continue;

                if (!wtx.value_map.count("to") || wtx.value_map["to"].empty())
                {
                    // Offline transaction
                    if (Consensus::IsConvergenceOutputScript(txout.scriptPubKey)) {
                        strHTML += "<b>" + tr("To") + ":</b> " +
                            tr("Convergence protocol (ORU committed to consensus convergence output; not a normal pay-to-address)") +
                            "<br>";
                    } else {
                        CTxDestination address;
                        if (ExtractDestination(txout.scriptPubKey, address))
                        {
                            strHTML += "<b>" + tr("To") + ":</b> ";
                            std::string name;
                            if (wallet.getAddress(
                                    address, &name, /* is_mine= */ nullptr, /* purpose= */ nullptr) && !name.empty())
                                strHTML += GUIUtil::HtmlEscape(name) + " ";
                            strHTML += GUIUtil::HtmlEscape(EncodeDestination(address));
                            if(toSelf == ISMINE_SPENDABLE)
                                strHTML += " (" + tr("own address") + ")";
                            else if(toSelf & ISMINE_WATCH_ONLY)
                                strHTML += " (" + tr("watch-only") + ")";
                            strHTML += "<br>";
                        }
                    }
                }

                const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                strHTML += "<b>" + tr("Debit") + ":</b> " +
                    BitcoinUnits::formatAmountHtmlForAsset(out_asset, -txout.nValue, unit) + "<br>";
                if(toSelf)
                    strHTML += "<b>" + tr("Credit") + ":</b> " +
                        BitcoinUnits::formatAmountHtmlForAsset(out_asset, txout.nValue, unit) + "<br>";
            }

            if (fAllToMe)
            {
                // Payment to self
                CAmount nChange = wtx.change;
                const CAmount self_debit = rec->amount_asset == ASSET_RES ? wtx.debit_res : wtx.debit_oru;
                const CAmount self_credit = rec->amount_asset == ASSET_RES ? wtx.credit_res : wtx.credit_oru;
                const CAmount nValue = self_credit - nChange;
                strHTML += "<b>" + tr("Total debit") + ":</b> " +
                    BitcoinUnits::formatAmountHtmlForAsset(rec->amount_asset, -(self_debit - nChange), unit) + "<br>";
                strHTML += "<b>" + tr("Total credit") + ":</b> " +
                    BitcoinUnits::formatAmountHtmlForAsset(rec->amount_asset, nValue, unit) + "<br>";
            }

            CAmount nTxFee = wtx.fee_oru;
            if (nTxFee > 0)
                strHTML += "<b>" + tr("Fee") + " (ORU):</b> " +
                    BitcoinUnits::formatFeeOruHtml(-nTxFee, unit) + "<br>";
            // Dual-pool RES sends can have both RES change and ORU fee change; list each.
            if ((wtx.debit_res > 0 || wtx.debit_oru > 0) && !fAllToMe) {
                CAmount change_oru = 0;
                CAmount change_res = 0;
                for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i) {
                    if (!wtx.txout_is_mine[i] || wtx.tx->vout[i].nValue <= 0) continue;
                    const AssetType a = AssetTypeFromScriptPubKey(wtx.tx->vout[i].scriptPubKey);
                    if (a == ASSET_ORU) change_oru += wtx.tx->vout[i].nValue;
                    else if (a == ASSET_RES) change_res += wtx.tx->vout[i].nValue;
                }
                if (change_res > 0) {
                    strHTML += "<b>" + tr("Change") + " (RES):</b> " +
                        BitcoinUnits::formatAmountHtmlForAsset(ASSET_RES, change_res, unit) + "<br>";
                }
                if (change_oru > 0) {
                    strHTML += "<b>" + tr("Change") + " (ORU):</b> " +
                        BitcoinUnits::formatAmountHtmlForAsset(ASSET_ORU, change_oru, unit) + "<br>";
                }
            } else if (wtx.change > 0) {
                AssetType change_asset = ASSET_ORU;
                for (unsigned int i = 0; i < wtx.tx->vout.size(); ++i) {
                    if (!wtx.txout_is_mine[i] || wtx.tx->vout[i].nValue <= 0) continue;
                    change_asset = AssetTypeFromScriptPubKey(wtx.tx->vout[i].scriptPubKey);
                    if (change_asset == ASSET_ORU) break;
                }
                QString change_label = tr("Change");
                if (change_asset == ASSET_ORU && (wtx.debit_res > 0 || rec->amount_asset == ASSET_RES)) {
                    change_label = tr("Change") + " (ORU)";
                } else if (change_asset == ASSET_RES) {
                    change_label = tr("Change") + " (RES)";
                }
                strHTML += "<b>" + change_label + ":</b> " +
                    BitcoinUnits::formatAmountHtmlForAsset(change_asset, wtx.change, unit) + "<br>";
            }
        }
        else
        {
            //
            // Mixed debit transaction
            //
            auto mine = wtx.txin_is_mine.begin();
            for (const CTxIn& txin : wtx.tx->vin) {
                if (*(mine++)) {
                    strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatHtmlWithUnit(unit, -wallet.getDebit(txin, ISMINE_ALL)) + "<br>";
                }
            }
            mine = wtx.txout_is_mine.begin();
            for (const CTxOut& txout : wtx.tx->vout) {
                if (*(mine++)) {
                    const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                    strHTML += "<b>" + tr("Credit") + ":</b> " +
                        BitcoinUnits::formatAmountHtmlForAsset(out_asset, wallet.getCredit(txout, ISMINE_ALL), unit) + "<br>";
                }
            }
        }
    }

    {
        const bool has_oru_flow = wtx.credit_oru != 0 || wtx.debit_oru != 0;
        const bool has_res_flow = wtx.credit_res != 0 || wtx.debit_res != 0;
        if (has_oru_flow) {
            strHTML += "<b>" + tr("Net amount") + " (ORU):</b> " +
                BitcoinUnits::formatAmountHtmlForAsset(ASSET_ORU, net_oru, unit, true) + "<br>";
        }
        if (has_res_flow) {
            strHTML += "<b>" + tr("Net amount") + " (RES):</b> " +
                BitcoinUnits::formatAmountHtmlForAsset(ASSET_RES, net_res, unit, true) + "<br>";
        }
        if (!has_oru_flow && !has_res_flow) {
            strHTML += "<b>" + tr("Net amount") + ":</b> " +
                BitcoinUnits::formatAmountHtmlForAsset(ASSET_ORU, 0, unit, true) + "<br>";
        }
    }

    //
    // Message
    //
    if (wtx.value_map.count("message") && !wtx.value_map["message"].empty())
        strHTML += "<br><b>" + tr("Message") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.value_map["message"], true) + "<br>";
    if (wtx.value_map.count("comment") && !wtx.value_map["comment"].empty())
        strHTML += "<br><b>" + tr("Comment") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.value_map["comment"], true) + "<br>";

    strHTML += "<b>" + tr("Transaction ID") + ":</b> " + rec->getTxHash() + "<br>";
    strHTML += "<b>" + tr("Transaction total size") + ":</b> " + QString::number(wtx.tx->GetTotalSize()) + " bytes<br>";
    strHTML += "<b>" + tr("Transaction size") + ":</b> " + QString::number(rigor::GetTransactionSizeBytes(*wtx.tx)) + " bytes<br>";
    strHTML += "<b>" + tr("Output index") + ":</b> " + QString::number(rec->getOutputIndex()) + "<br>";

    // Message from normal rigor:URI (rigor:123...?message=example)
    for (const std::pair<std::string, std::string>& r : orderForm) {
        if (r.first == "Message")
            strHTML += "<br><b>" + tr("Message") + ":</b><br>" + GUIUtil::HtmlEscape(r.second, true) + "<br>";

        //
        // PaymentRequest info:
        //
        if (r.first == "PaymentRequest")
        {
            QString merchant;
            if (!GetPaymentRequestMerchant(r.second, merchant)) {
                merchant.clear();
            } else {
                merchant += tr(" (Certificate was not verified)");
            }
            if (!merchant.isNull()) {
                strHTML += "<b>" + tr("Merchant") + ":</b> " + GUIUtil::HtmlEscape(merchant) + "<br>";
            }
        }
    }

    if (wtx.is_coinbase)
    {
        quint32 numBlocksToMaturity = Params().GetConsensus().nCoinbaseMaturity + 1;
        strHTML += "<br>" + tr("Coinbase rewards must mature %1 blocks before they can be spent. When this block was produced, it was broadcast to the network to be added to the block chain. If it fails to get into the chain, its state will change to \"not accepted\" and it won't be spendable. This may occasionally happen if another node produces a block within a few seconds of yours.").arg(QString::number(numBlocksToMaturity)) + "<br>";
    }

    //
    // Debug view
    //
    if (node.getLogCategories() != BCLog::NONE)
    {
        strHTML += "<hr><br>" + tr("Debug information") + "<br><br>";
        for (const CTxIn& txin : wtx.tx->vin)
            if(wallet.txinIsMine(txin))
                strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatHtmlWithUnit(unit, -wallet.getDebit(txin, ISMINE_ALL)) + "<br>";
        for (const CTxOut& txout : wtx.tx->vout)
            if(wallet.txoutIsMine(txout)) {
                const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
                strHTML += "<b>" + tr("Credit") + ":</b> " +
                    BitcoinUnits::formatAmountHtmlForAsset(out_asset, wallet.getCredit(txout, ISMINE_ALL), unit) + "<br>";
            }

        strHTML += "<br><b>" + tr("Transaction") + ":</b><br>";
        strHTML += GUIUtil::HtmlEscape(wtx.tx->ToString(), true);

        strHTML += "<br><b>" + tr("Inputs") + ":</b>";
        strHTML += "<ul>";

        for (const CTxIn& txin : wtx.tx->vin)
        {
            COutPoint prevout = txin.prevout;

            Coin prev;
            if(node.getUnspentOutput(prevout, prev))
            {
                {
                    strHTML += "<li>";
                    const CTxOut &vout = prev.out;
                    CTxDestination address;
                    if (ExtractDestination(vout.scriptPubKey, address))
                    {
                        std::string name;
                        if (wallet.getAddress(address, &name, /* is_mine= */ nullptr, /* purpose= */ nullptr) && !name.empty())
                            strHTML += GUIUtil::HtmlEscape(name) + " ";
                        strHTML += QString::fromStdString(EncodeDestination(address));
                    }
                    const AssetType in_asset = AssetTypeFromScriptPubKey(vout.scriptPubKey);
                    strHTML = strHTML + " " + tr("Amount") + "=" +
                        BitcoinUnits::formatAmountHtmlForAsset(in_asset, vout.nValue, unit);
                    strHTML = strHTML + " IsMine=" + (wallet.txoutIsMine(vout) & ISMINE_SPENDABLE ? tr("true") : tr("false")) + "</li>";
                    strHTML = strHTML + " IsWatchOnly=" + (wallet.txoutIsMine(vout) & ISMINE_WATCH_ONLY ? tr("true") : tr("false")) + "</li>";
                }
            }
        }

        strHTML += "</ul>";
    }

    strHTML += "</font></html>";
    return strHTML;
}
