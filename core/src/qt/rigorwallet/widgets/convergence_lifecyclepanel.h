// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_LIFECYCLEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_LIFECYCLEPANEL_H

#include <QFrame>

class ClientModel;
class QLabel;
class QProgressBar;
class WalletModel;

namespace RigorWallet {

/**
 * User-facing RES Convergence lifecycle (W=32 progress + result).
 * TASK-WALLET-CONVERGENCE-UI-USER-SIMPLIFICATION-001 — Bitcoin-simple UX.
 * Countdown is tip-height based only — never wall-clock.
 */
class ConvergenceLifecyclePanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceLifecyclePanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};

    QLabel* m_titleLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_detailLabel{nullptr};
    QLabel* m_windowLabel{nullptr};
    QLabel* m_startLabel{nullptr};
    QLabel* m_currentLabel{nullptr};
    QLabel* m_finalLabel{nullptr};
    QLabel* m_remainingLabel{nullptr};
    QLabel* m_resultLabel{nullptr};
    QProgressBar* m_progress{nullptr};
};

} // namespace RigorWallet

#endif
