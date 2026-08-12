// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_CONVERGENCE_WALLET_DATA_H
#define BITCOIN_QT_RIGORWALLET_CONVERGENCE_WALLET_DATA_H

#include <interfaces/wallet.h>
#include <wallet/convergence_window_ui.h>

#include <QString>
#include <vector>

class WalletModel;

namespace RigorWallet {

/** User-facing convergence lifecycle — maps wallet metadata + tip height (no consensus changes). */
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
    int block_height{-1}; //!< inclusion / start block
    ConvergenceUserStatus status{ConvergenceUserStatus::Pending};
    QString status_label;
    QString linked_mint_txid;
    CAmount oru_amount{0};
    CAmount res_amount{0};
    int result_block{-1};
    wallet::ConvergenceUiWindowSnapshot window{};
    wallet::ConvergenceUiLifecycle lifecycle{wallet::ConvergenceUiLifecycle::Submitted};
};

class ConvergenceWalletData
{
public:
    ConvergenceWalletData() = delete;

    static ConvergenceUserStatus mapStateString(const QString& state);
    static QString statusLabel(ConvergenceUserStatus status);

    /** @param tip_height Active chain tip for W=32 countdown (-1 if unknown). */
    static std::vector<ConvergenceHistoryRow> buildHistory(WalletModel* walletModel, int tip_height = -1);
    static int countByStatus(const std::vector<ConvergenceHistoryRow>& rows, ConvergenceUserStatus status);
    static int historicalCount(const std::vector<ConvergenceHistoryRow>& rows);
    static const ConvergenceHistoryRow* latestRow(const std::vector<ConvergenceHistoryRow>& rows);
    static const ConvergenceHistoryRow* pendingRow(const std::vector<ConvergenceHistoryRow>& rows);
};

} // namespace RigorWallet

#endif
