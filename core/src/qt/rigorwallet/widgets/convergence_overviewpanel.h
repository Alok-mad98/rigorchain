// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_OVERVIEWPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_OVERVIEWPANEL_H

#include <QFrame>

class ClientModel;
class QLabel;
class WalletModel;

namespace RigorWallet {

/** Convergence activity summary (BUILD-18). */
class ConvergenceOverviewPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceOverviewPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};
    QLabel* m_activityLabel{nullptr};
    QLabel* m_historicalLabel{nullptr};
    QLabel* m_latestLabel{nullptr};
    QLabel* m_pendingLabel{nullptr};
};

} // namespace RigorWallet

#endif
