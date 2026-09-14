// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_WIZARD_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_WIZARD_H

#include <consensus/amount.h>

#include <QDialog>

class BitcoinAmountField;
class ClientModel;
class QLabel;
class QStackedWidget;
class WalletModel;

namespace RigorWallet {

/** Step-by-step convergence workflow (BUILD-20). */
class ConvergenceWizard : public QDialog
{
    Q_OBJECT

public:
    ConvergenceWizard(WalletModel* walletModel, ClientModel* clientModel, QWidget* parent = nullptr);

private Q_SLOTS:
    void onBack();
    void onNext();
    void onSubmit();

private:
    void buildSteps();
    void showStep(int index);
    void refreshReviewStep();
    bool validateAmount(CAmount& amount_out);
    CAmount minMintBurn() const;

    WalletModel* m_walletModel{nullptr};
    ClientModel* m_clientModel{nullptr};
    QStackedWidget* m_stack{nullptr};
    BitcoinAmountField* m_amountField{nullptr};
    QLabel* m_reviewLabel{nullptr};
    QLabel* m_confirmLabel{nullptr};
    int m_step{0};
};

} // namespace RigorWallet

#endif
