// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCESTATUSPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCESTATUSPANEL_H

#include <QFrame>

class QLabel;
class WalletModel;

namespace RigorWallet {

/** Convergence status summary for Dashboard (BUILD-11). Node-authoritative only. */
class ConvergenceStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceStatusPanel(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);
    void refresh();

private:
    void applyTheme();

    WalletModel* m_walletModel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_detailLabel{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCESTATUSPANEL_H
