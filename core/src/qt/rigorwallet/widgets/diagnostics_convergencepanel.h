// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_CONVERGENCEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_CONVERGENCEPANEL_H

#include <QFrame>

class ClientModel;
class WalletModel;

class QLabel;
class QTextEdit;

namespace RigorWallet {

class StatusBadge;

/**
 * Settings → Advanced → System Diagnostics → Convergence Details.
 * Holds protocol / debugging fields removed from the user Convergence page.
 * Presentation only — no consensus changes.
 */
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

    // Summary
    QLabel* m_pendingLabel{nullptr};
    QLabel* m_completedLabel{nullptr};
    QLabel* m_mintLabel{nullptr};
    QLabel* m_lagLabel{nullptr};

    // Consensus Information
    QLabel* m_windowWLabel{nullptr};
    QLabel* m_selectionTargetLabel{nullptr};
    QLabel* m_resTotalLabel{nullptr};
    QLabel* m_windowProgressLabel{nullptr};
    QLabel* m_convergenceSumLabel{nullptr};
    QLabel* m_lastWinnerLabel{nullptr};
    QLabel* m_tipHeightLabel{nullptr};
    QLabel* m_lastResHeightLabel{nullptr};

    // Round / candidate diagnostics
    QLabel* m_tipRoundLabel{nullptr};
    QLabel* m_prevRoundLabel{nullptr};

    // Draw bind / digests
    QLabel* m_drawNextLabel{nullptr};
    QLabel* m_drawBindHeightLabel{nullptr};
    QLabel* m_drawBindHashLabel{nullptr};

    // Network / debug
    QLabel* m_peersLabel{nullptr};
    QLabel* m_netHeightLabel{nullptr};
    QLabel* m_syncLabel{nullptr};
    QLabel* m_historicalCountLabel{nullptr};
    QLabel* m_latestTxLabel{nullptr};
    QLabel* m_validationLabel{nullptr};

    QTextEdit* m_educationView{nullptr};
};

} // namespace RigorWallet

#endif
