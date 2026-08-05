// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/convergence/convergence_wallet_data.h>

#include <qt/walletmodel.h>

#include <wallet/tx_semantic.h>

#include <uint256.h>

#include <algorithm>
#include <map>
#include <set>

#include <QCoreApplication>

namespace RigorWallet {

ConvergenceUserStatus ConvergenceWalletData::mapStateString(const QString& state)
{
    if (state == QLatin1String("BROADCASTED") || state == QLatin1String("CREATED")) {
        return ConvergenceUserStatus::Pending;
    }
    if (state == QLatin1String("INCLUDED")) {
        return ConvergenceUserStatus::Included;
    }
    if (state == QLatin1String("MATURING") || state == QLatin1String("LOST")) {
        return ConvergenceUserStatus::Completed;
    }
    if (state == QLatin1String("WON_PENDING") || state == QLatin1String("WON_CONFIRMED")) {
        return ConvergenceUserStatus::Minted;
    }
    return ConvergenceUserStatus::Failed;
}

QString ConvergenceWalletData::statusLabel(ConvergenceUserStatus status)
{
    switch (status) {
    case ConvergenceUserStatus::Pending:
        return QCoreApplication::translate("ConvergenceWalletData", "Pending");
    case ConvergenceUserStatus::Included:
        return QCoreApplication::translate("ConvergenceWalletData", "Included");
    case ConvergenceUserStatus::Completed:
        return QCoreApplication::translate("ConvergenceWalletData", "Completed");
    case ConvergenceUserStatus::Minted:
        return QCoreApplication::translate("ConvergenceWalletData", "Minted");
    case ConvergenceUserStatus::Failed:
        return QCoreApplication::translate("ConvergenceWalletData", "Failed");
    }
    return QCoreApplication::translate("ConvergenceWalletData", "Unknown");
}

static bool isConvergenceWalletTx(const interfaces::WalletTx& wtx)
{
    if (wtx.semantic == wallet::TxSemanticKind::OruConvergence) return true;
    const auto it = wtx.value_map.find("convergence_v2");
    return it != wtx.value_map.end() && it->second == "1";
}

static QString mapValueStr(const interfaces::WalletTx& wtx, const char* key)
{
    const auto it = wtx.value_map.find(key);
    return it != wtx.value_map.end() ? QString::fromStdString(it->second) : QString();
}

static int mintBlockHeight(WalletModel* walletModel, const uint256& txid)
{
    interfaces::WalletTxStatus status;
    interfaces::WalletOrderForm order;
    bool in_mempool = false;
    int num_blocks = 0;
    walletModel->wallet().getWalletTxDetails(txid, status, order, in_mempool, num_blocks);
    if (status.block_height >= 0) return status.block_height;
    return -1;
}

std::vector<ConvergenceHistoryRow> ConvergenceWalletData::buildHistory(WalletModel* walletModel)
{
    std::vector<ConvergenceHistoryRow> rows;
    if (!walletModel) return rows;

    std::map<int, QString> mint_by_height;
    const std::set<interfaces::WalletTx> txs = walletModel->wallet().getWalletTxs();
    for (const interfaces::WalletTx& wtx : txs) {
        if (wtx.semantic != wallet::TxSemanticKind::ResMint) continue;
        const uint256 hash = wtx.tx->GetHash();
        const int h = mintBlockHeight(walletModel, hash);
        if (h >= 0) {
            mint_by_height[h] = QString::fromStdString(hash.GetHex());
        }
    }

    for (const interfaces::WalletTx& wtx : txs) {
        if (!isConvergenceWalletTx(wtx)) continue;
        ConvergenceHistoryRow row;
        row.txid = QString::fromStdString(wtx.tx->GetHash().GetHex());
        row.timestamp = wtx.time;
        row.oru_amount = -wtx.debit_oru;
        const QString state = mapValueStr(wtx, "convergence_state");
        row.status = state.isEmpty() ? ConvergenceUserStatus::Pending : mapStateString(state);
        row.status_label = statusLabel(row.status);
        const QString inc = mapValueStr(wtx, "convergence_included_height");
        if (!inc.isEmpty()) row.block_height = inc.toInt();
        if (row.block_height >= 0) {
            const auto it = mint_by_height.find(row.block_height);
            if (it != mint_by_height.end()) {
                row.linked_mint_txid = it->second;
                if (row.status == ConvergenceUserStatus::Completed ||
                    row.status == ConvergenceUserStatus::Included) {
                    row.status = ConvergenceUserStatus::Minted;
                    row.status_label = statusLabel(row.status);
                }
            }
        }
        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(), [](const ConvergenceHistoryRow& a, const ConvergenceHistoryRow& b) {
        return a.timestamp > b.timestamp;
    });
    return rows;
}

int ConvergenceWalletData::countByStatus(const std::vector<ConvergenceHistoryRow>& rows, ConvergenceUserStatus status)
{
    return static_cast<int>(std::count_if(rows.begin(), rows.end(), [status](const ConvergenceHistoryRow& r) {
        return r.status == status;
    }));
}

int ConvergenceWalletData::historicalCount(const std::vector<ConvergenceHistoryRow>& rows)
{
    return static_cast<int>(rows.size());
}

const ConvergenceHistoryRow* ConvergenceWalletData::latestRow(const std::vector<ConvergenceHistoryRow>& rows)
{
    return rows.empty() ? nullptr : &rows.front();
}

const ConvergenceHistoryRow* ConvergenceWalletData::pendingRow(const std::vector<ConvergenceHistoryRow>& rows)
{
    for (const ConvergenceHistoryRow& r : rows) {
        if (r.status == ConvergenceUserStatus::Pending) return &r;
    }
    return nullptr;
}

} // namespace RigorWallet
