// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_ORUBALANCEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_ORUBALANCEPANEL_H

#include <interfaces/wallet.h>

#include <qt/bitcoinunits.h>

#include <QFrame>

class QLabel;

namespace RigorWallet {

/** Dedicated ORU balance panel — never mixes RES (BUILD-09). */
class ORUBalancePanel : public QFrame
{
    Q_OBJECT

public:
    explicit ORUBalancePanel(QWidget* parent = nullptr);

    void setBalances(const interfaces::WalletBalances& balances, BitcoinUnit unit, bool privacy);

private:
    void applyTheme();
    void setRow(int row, const QString& title, QLabel* valueLabel, const QString& amount);

    QLabel* m_titleLabel{nullptr};
    QLabel* m_confirmedLabel{nullptr};
    QLabel* m_unconfirmedLabel{nullptr};
    QLabel* m_pendingLabel{nullptr};
    QLabel* m_totalLabel{nullptr};
    QLabel* m_confirmedValue{nullptr};
    QLabel* m_unconfirmedValue{nullptr};
    QLabel* m_pendingValue{nullptr};
    QLabel* m_totalValue{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_ORUBALANCEPANEL_H
