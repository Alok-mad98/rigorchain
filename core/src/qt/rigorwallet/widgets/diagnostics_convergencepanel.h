// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_CONVERGENCEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_CONVERGENCEPANEL_H

#include <QFrame>

class ClientModel;
class WalletModel;

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Convergence diagnostics panel (BUILD-31). */
class DiagnosticsConvergencePanel : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsConvergencePanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};
    StatusBadge* m_badge{nullptr};
    QLabel* m_pendingLabel{nullptr};
    QLabel* m_completedLabel{nullptr};
    QLabel* m_mintLabel{nullptr};
    QLabel* m_lagLabel{nullptr};
};

} // namespace RigorWallet

#endif
