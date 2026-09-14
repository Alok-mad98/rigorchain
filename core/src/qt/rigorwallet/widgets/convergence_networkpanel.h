// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_NETWORKPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_NETWORKPANEL_H

#include <QFrame>

class ClientModel;
class QLabel;

namespace RigorWallet {

/**
 * Wallet → Convergence → Network Statistics (TASK-WALLET-CONVERGENCE-NETWORK-STATS-001).
 * Displays chain-wide stats from getconvergencestatus. No protocol digests.
 */
class ConvergenceNetworkPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceNetworkPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    QLabel* m_priceLabel{nullptr};
    QLabel* m_nextPriceLabel{nullptr};
    QLabel* m_resLabel{nullptr};
    QLabel* m_updatedLabel{nullptr};
};

} // namespace RigorWallet

#endif
