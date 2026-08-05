// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_HISTORYPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_HISTORYPANEL_H

#include <QFrame>

class QStackedWidget;
class QTableWidget;
class WalletModel;

namespace RigorWallet {

class EmptyStateWidget;

/** Convergence history with mint linking (BUILD-22 / BUILD-23). */
class ConvergenceHistoryPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceHistoryPanel(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);
    void refresh();

private:
    WalletModel* m_walletModel{nullptr};
    QStackedWidget* m_stack{nullptr};
    EmptyStateWidget* m_empty{nullptr};
    QTableWidget* m_table{nullptr};
};

} // namespace RigorWallet

#endif
