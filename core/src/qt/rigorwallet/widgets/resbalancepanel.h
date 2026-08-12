// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_RESBALANCEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_RESBALANCEPANEL_H

#include <interfaces/wallet.h>

#include <QFrame>

class QLabel;
class WalletModel;

namespace RigorWallet {

/** Dedicated RES balance panel — never mixes ORU (BUILD-10). */
class RESBalancePanel : public QFrame
{
    Q_OBJECT

public:
    explicit RESBalancePanel(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);
    void setBalances(const interfaces::WalletBalances& balances, bool privacy);

private:
    void applyTheme();

    WalletModel* m_walletModel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_confirmedValue{nullptr};
    QLabel* m_pendingValue{nullptr};
    QLabel* m_totalValue{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_RESBALANCEPANEL_H
