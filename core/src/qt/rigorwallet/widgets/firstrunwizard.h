// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H

#include <QDialog>

class ClientModel;
class WalletModel;
class QLabel;
class QStackedWidget;

namespace RigorWallet {

/** First-launch readiness checks — presentation only, no backend changes.
 *  Implemented as QDialog (not QWizard): depends Qt is built with -no-feature-wizard.
 */
class FirstRunWizard : public QDialog
{
    Q_OBJECT

public:
    FirstRunWizard(ClientModel* clientModel, WalletModel* walletModel, QWidget* parent = nullptr);

    static bool shouldShow();
    static void markCompleted();

private Q_SLOTS:
    void onBack();
    void onNext();
    void onFinish();

private:
    void buildPages();
    void showStep(int index);
    void refreshStatusPage();

    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};
    QStackedWidget* m_stack{nullptr};
    QLabel* m_nodeStatus{nullptr};
    QLabel* m_explorerStatus{nullptr};
    QLabel* m_convergenceStatus{nullptr};
    int m_step{0};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_FIRSTRUNWIZARD_H
