// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H

#include <QWizard>

class ClientModel;
class WalletModel;
class QLabel;

namespace RigorWallet {

/** First-launch readiness checks — presentation only, no backend changes. */
class FirstRunWizard : public QWizard
{
    Q_OBJECT

public:
    FirstRunWizard(ClientModel* clientModel, WalletModel* walletModel, QWidget* parent = nullptr);

    static bool shouldShow();
    static void markCompleted();

private:
    void buildPages();
    void refreshStatusPage();

    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};
    QLabel* m_nodeStatus{nullptr};
    QLabel* m_explorerStatus{nullptr};
    QLabel* m_convergenceStatus{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H
