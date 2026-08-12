// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_wizard.h>

#include <consensus/convergenceres.h>
#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/rigorwallet/convergence/convergence_education.h>
#include <qt/walletmodel.h>

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceWizard::ConvergenceWizard(WalletModel* walletModel, ClientModel* clientModel, QWidget* parent)
    : QDialog(parent),
      m_walletModel(walletModel),
      m_clientModel(clientModel)
{
    setWindowTitle(tr("Convergence Wizard"));
    setMinimumWidth(480);
    buildSteps();
}

void ConvergenceWizard::buildSteps()
{
    auto* layout = new QVBoxLayout(this);
    m_stack = new QStackedWidget(this);

    // Step 1 — amount
    auto* step1 = new QWidget(this);
    auto* s1 = new QVBoxLayout(step1);
    s1->addWidget(new QLabel(tr("Step 1: Select ORU amount to convergence"), step1));
    s1->addWidget(new QLabel(ConvergenceEducation::whatIsConvergence(), step1));
    m_amountField = new BitcoinAmountField(step1);
    // Convergence minimum is 1 ORU; lock display to ORU and hide mORU/uORU/Tharn.
    m_amountField->setDisplayUnit(BitcoinUnit::ORU);
    m_amountField->setUnitSelectorVisible(false);
    m_amountField->SetMinValue(Consensus::MIN_CONVERGENCE_AMOUNT);
    m_amountField->setSingleStep(COIN);
    s1->addWidget(m_amountField);
    m_stack->addWidget(step1);

    // Step 2 — review
    auto* step2 = new QWidget(this);
    auto* s2 = new QVBoxLayout(step2);
    s2->addWidget(new QLabel(tr("Step 2: Review convergence parameters"), step2));
    m_reviewLabel = new QLabel(step2);
    m_reviewLabel->setWordWrap(true);
    s2->addWidget(m_reviewLabel);
    m_stack->addWidget(step2);

    // Step 3 — confirm
    auto* step3 = new QWidget(this);
    auto* s3 = new QVBoxLayout(step3);
    s3->addWidget(new QLabel(tr("Step 3: Confirmation"), step3));
    m_confirmLabel = new QLabel(
        tr("Convergence permanently commits ORU under consensus rules. A RES mint may follow deterministic selection but is never guaranteed."),
        step3);
    m_confirmLabel->setWordWrap(true);
    s3->addWidget(m_confirmLabel);
    m_stack->addWidget(step3);

    // Step 4 — submit
    auto* step4 = new QWidget(this);
    auto* s4 = new QVBoxLayout(step4);
    s4->addWidget(new QLabel(tr("Step 4: Submit"), step4));
    s4->addWidget(new QLabel(tr("Press Finish to broadcast the convergence transaction."), step4));
    m_stack->addWidget(step4);

    layout->addWidget(m_stack);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* backBtn = buttons->addButton(tr("Back"), QDialogButtonBox::ResetRole);
    QPushButton* nextBtn = buttons->addButton(tr("Next"), QDialogButtonBox::AcceptRole);
    QPushButton* finishBtn = buttons->addButton(tr("Finish"), QDialogButtonBox::ApplyRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(backBtn, &QPushButton::clicked, this, &ConvergenceWizard::onBack);
    connect(nextBtn, &QPushButton::clicked, this, &ConvergenceWizard::onNext);
    connect(finishBtn, &QPushButton::clicked, this, &ConvergenceWizard::onSubmit);
    layout->addWidget(buttons);

    showStep(0);
}

void ConvergenceWizard::showStep(int index)
{
    m_step = index;
    m_stack->setCurrentIndex(index);
    if (index == 1) refreshReviewStep();
}

void ConvergenceWizard::refreshReviewStep()
{
    CAmount amt{};
    if (!validateAmount(amt)) {
        m_reviewLabel->setText(tr("Enter a valid amount in Step 1."));
        return;
    }
    m_reviewLabel->setText(tr("Amount: %1\nMinimum: %2\nNetwork fee: none for convergence\nParameters are reported by your node.")
                               .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, amt))
                               .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, Consensus::MIN_CONVERGENCE_AMOUNT)));
}

bool ConvergenceWizard::validateAmount(CAmount& amount_out)
{
    if (!m_walletModel || !m_amountField) return false;
    bool ok = false;
    amount_out = m_amountField->value(&ok);
    return ok && amount_out > 0;
}

void ConvergenceWizard::onBack()
{
    if (m_step > 0) showStep(m_step - 1);
}

void ConvergenceWizard::onNext()
{
    if (m_step == 0) {
        CAmount amt{};
        if (!validateAmount(amt)) {
            QMessageBox::warning(this, tr("Convergence"), tr("Enter a valid ORU amount."));
            return;
        }
        if (amt < Consensus::MIN_CONVERGENCE_AMOUNT) {
            QMessageBox::warning(this, tr("Convergence"), tr("Amount is below the minimum convergence."));
            return;
        }
    }
    if (m_step < m_stack->count() - 1) showStep(m_step + 1);
}

void ConvergenceWizard::onSubmit()
{
    if (!m_walletModel || !m_clientModel) return;
    WalletModel::UnlockContext ctx(m_walletModel->requestUnlock());
    if (!ctx.isValid()) {
        QMessageBox::warning(this, tr("Wallet locked"), tr("Unlock the wallet to submit convergence."));
        return;
    }
    CAmount amt{};
    if (!validateAmount(amt)) {
        QMessageBox::warning(this, tr("Convergence"), tr("Invalid amount."));
        return;
    }
    const auto balances = m_walletModel->getCachedBalance();
    const CAmount oru_avail = balances.oru_balance + balances.oru_unconfirmed_balance;
    if (amt > oru_avail) {
        QMessageBox::warning(this, tr("Insufficient ORU"),
            tr("Insufficient ORU available. Fund the wallet or lower the amount."));
        return;
    }
    const auto ret = m_walletModel->sendConvergenceOru(amt);
    if (ret.status == WalletModel::OK) {
        QMessageBox::information(this, tr("Convergence"),
            tr("Convergence transaction broadcast. Track status in Convergence History."));
        accept();
    }
}

} // namespace RigorWallet
