// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_CONVERGENCE_WALLET_DATA_H
#define BITCOIN_QT_RIGORWALLET_CONVERGENCE_WALLET_DATA_H

#include <interfaces/wallet.h>

#include <QString>
#include <vector>

class WalletModel;

namespace RigorWallet {

/** User-facing convergence lifecycle (BUILD-21) — maps wallet metadata only. */
enum class ConvergenceUserStatus {
    Pending,
    Included,
    Completed,
    Minted,
    Failed,
};

struct ConvergenceHistoryRow {
    QString txid;
    qint64 timestamp{0};
    int block_height{-1};
    ConvergenceUserStatus status{ConvergenceUserStatus::Pending};
    QString status_label;
    QString linked_mint_txid;
    CAmount oru_amount{0};
};

class ConvergenceWalletData
{
public:
    ConvergenceWalletData() = delete;

    static ConvergenceUserStatus mapStateString(const QString& state);
    static QString statusLabel(ConvergenceUserStatus status);

    static std::vector<ConvergenceHistoryRow> buildHistory(WalletModel* walletModel);
    static int countByStatus(const std::vector<ConvergenceHistoryRow>& rows, ConvergenceUserStatus status);
    static int historicalCount(const std::vector<ConvergenceHistoryRow>& rows);
    static const ConvergenceHistoryRow* latestRow(const std::vector<ConvergenceHistoryRow>& rows);
    static const ConvergenceHistoryRow* pendingRow(const std::vector<ConvergenceHistoryRow>& rows);
};

} // namespace RigorWallet

#endif
