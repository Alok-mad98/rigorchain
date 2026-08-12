// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_STARTPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_STARTPANEL_H

#include <consensus/amount.h>

#include <QFrame>

class BitcoinAmountField;
class ClientModel;
class QLabel;
class QPushButton;
class WalletModel;

namespace RigorWallet {

/**
 * Wallet → Convergence → Start Convergence (TASK-WALLET-CONVERGENCE-START-UI-FINALIZATION-001).
 * Bitcoin-simple spend UX: ORU amount → W=32 window → possible RES. No consensus internals.
 */
class ConvergenceStartPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceStartPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);
    void refresh();

Q_SIGNALS:
    void convergenceSubmitted();

private Q_SLOTS:
    void onAmountChanged();
    void onStartClicked();

private:
    enum class AmountValidity {
        Empty,
        BelowMinimum,
        Insufficient,
        Valid,
    };

    void updateBalance();
    void updateValidation();
    void updateEstimate();
    void updateNetworkGate();
    AmountValidity checkAmount(CAmount& amount_out) const;
    CAmount availableOru() const;
    bool networkReady(QString* warn_out) const;

    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};

    QLabel* m_titleLabel{nullptr};
    QLabel* m_subtitleLabel{nullptr};
    QLabel* m_offlineLabel{nullptr};
    QLabel* m_balanceTitle{nullptr};
    QLabel* m_balanceValue{nullptr};
    QLabel* m_amountTitle{nullptr};
    BitcoinAmountField* m_amountField{nullptr};
    QLabel* m_validationLabel{nullptr};
    QLabel* m_windowTitle{nullptr};
    QLabel* m_windowValue{nullptr};
    QLabel* m_estimateTitle{nullptr};
    QLabel* m_estimateRemaining{nullptr};
    QLabel* m_estimateTime{nullptr};
    QLabel* m_resultTitle{nullptr};
    QLabel* m_resultBody{nullptr};
    QPushButton* m_startButton{nullptr};
};

} // namespace RigorWallet

#endif
