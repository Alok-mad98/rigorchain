// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_wizard.h>

#include <consensus/convergenceres.h>
#include <consensus/res_issuance.h>
#include <wallet/mint_height_pad.h>
#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#include <interfaces/node.h>
#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/rigorwallet/convergence/convergence_education.h>
#include <qt/walletmodel.h>

#include <univalue.h>

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
    m_amountField->setObjectName(QStringLiteral("wizardAmountField"));
    // Mint UI: floor is ResMinMintBurnAtoms(h), not 1 ORU. Lock display to ORU.
    m_amountField->setDisplayUnit(BitcoinUnit::ORU);
    m_amountField->setUnitSelectorVisible(false);
    const CAmount min_mint = minMintBurn();
    m_amountField->SetMinValue(min_mint);
    m_amountField->setSingleStep(min_mint);
    m_amountField->setValue(min_mint);
    s1->addWidget(m_amountField);
    m_stack->addWidget(step1);

    // Step 2 — review
    auto* step2 = new QWidget(this);
    auto* s2 = new QVBoxLayout(step2);
    s2->addWidget(new QLabel(tr("Step 2: Review convergence parameters"), step2));
    m_reviewLabel = new QLabel(step2);
    m_reviewLabel->setObjectName(QStringLiteral("wizardReviewLabel"));
    m_reviewLabel->setWordWrap(true);
    s2->addWidget(m_reviewLabel);
    m_stack->addWidget(step2);

    // Step 3 — confirm
    auto* step3 = new QWidget(this);
    auto* s3 = new QVBoxLayout(step3);
    s3->addWidget(new QLabel(tr("Step 3: Confirmation"), step3));
    m_confirmLabel = new QLabel(
        tr("This destroys ORU permanently to mint RES at the current protocol cost. The ORU cannot be recovered."),
        step3);
    m_confirmLabel->setObjectName(QStringLiteral("wizardConfirmLabel"));
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
    backBtn->setObjectName(QStringLiteral("wizardBackButton"));
    QPushButton* nextBtn = buttons->addButton(tr("Next"), QDialogButtonBox::AcceptRole);
    nextBtn->setObjectName(QStringLiteral("wizardNextButton"));
    QPushButton* finishBtn = buttons->addButton(tr("Finish"), QDialogButtonBox::ApplyRole);
    finishBtn->setObjectName(QStringLiteral("wizardFinishButton"));
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
    bool authorized = false;
    int confirm_height = -1;
    if (m_clientModel) {
        try {
            UniValue r = m_clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
            if (r.isObject() && r.exists("confirm_height")) {
                confirm_height = r["confirm_height"].getInt<int>();
                authorized = r.exists("issuance_authorized")
                    ? r["issuance_authorized"].get_bool()
                    : (confirm_height >= 0);
            }
        } catch (...) {
            authorized = false;
        }
    }
    if (!authorized || confirm_height < 0) {
        m_reviewLabel->setText(tr("Unable to read the confirming-height mint cost from the node."));
        return;
    }
    const int current_height = m_clientModel ? m_clientModel->getNumBlocks() : confirm_height - 1;
    const int target_height = wallet::ConvergenceTargetHeight(current_height);
    const CAmount minted = Consensus::ResMintAtomsFromBurn(amt, target_height);
    m_reviewLabel->setText(
        tr("ORU destroyed: %1\n"
           "RES this transaction will create: %2\n"
           "The wallet constructs the mint with a future confirmation-height safety margin so waiting to confirm does not void it.\n"
           "Destroyed ORU cannot be recovered.\n"
           "Minted RES unlocks after the reorg-protection lock.")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, amt))
            .arg(BitcoinUnits::formatRes(minted) + QStringLiteral(" RES")));
}

bool ConvergenceWizard::validateAmount(CAmount& amount_out)
{
    if (!m_walletModel || !m_amountField) return false;
    bool ok = false;
    amount_out = m_amountField->value(&ok);
    return ok && amount_out > 0;
}

CAmount ConvergenceWizard::minMintBurn() const
{
    if (m_clientModel) {
        try {
            UniValue r = m_clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
            if (r.isObject() && r.exists("confirm_height")) {
                const int current_height = m_clientModel->getNumBlocks();
                return Consensus::ResMinMintBurnAtoms(wallet::ConvergenceTargetHeight(current_height));
            }
        } catch (...) {
        }
    }
    return Consensus::ResMinMintBurnAtoms(0);
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
        if (amt < minMintBurn()) {
            QMessageBox::warning(this, tr("Convergence"),
                tr("Amount is below the ORU that mints 1 RES atom (%1).")
                    .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, minMintBurn())));
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
            tr("Mint broadcast. The ORU is destroyed; minted RES unlocks after the reorg-protection lock."));
        accept();
    } else if (ret.status == WalletModel::TransactionCommitFailed) {
        QMessageBox::critical(this, tr("Convergence"),
            tr("The node rejected the mint and it has not been broadcast. Reason: %1. No ORU was destroyed and the coins are still available.")
                .arg(ret.reasonCommitFailed));
    }
}

} // namespace RigorWallet
