// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_startpanel.h>

#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <interfaces/node.h>
#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/walletmodel.h>

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace RigorWallet {
namespace {

QString FormatDurationApprox(int64_t seconds)
{
    if (seconds < 60) {
        return QObject::tr("about %1 seconds").arg(seconds);
    }
    const int64_t minutes = (seconds + 59) / 60;
    if (minutes < 60) {
        return QObject::tr("about %1 minutes").arg(minutes);
    }
    const int64_t hours = (minutes + 59) / 60;
    if (hours < 48) {
        return QObject::tr("about %1 hours").arg(hours);
    }
    const int64_t days = (hours + 23) / 24;
    return QObject::tr("about %1 days").arg(days);
}

} // namespace

ConvergenceStartPanel::ConvergenceStartPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceStart"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorConvergenceStart {"
                      " background: %1; border: 1px solid %2; border-radius: 8px; padding: 12px; }"
                      "QFrame#RigorConvergenceStart QLabel#labelStartTitle {"
                      " font-weight: 600; font-size: 15px; color: %3; }"
                      "QFrame#RigorConvergenceStart QLabel#labelStartSubtitle {"
                      " color: %4; font-size: 12px; }"
                      "QFrame#RigorConvergenceStart QLabel#labelStartSection {"
                      " font-weight: 600; font-size: 12px; color: %3; margin-top: 6px; }"
                      "QFrame#RigorConvergenceStart QLabel#labelStartMuted {"
                      " color: %4; font-size: 12px; }"
                      "QFrame#RigorConvergenceStart QLabel#labelStartOffline {"
                      " color: %5; font-size: 12px; }"
                      "QFrame#RigorConvergenceStart QPushButton#pushButtonStartConvergence {"
                      " background-color: %6; color: #FFFFFF; font-weight: 600;"
                      " border: none; border-radius: 6px; min-height: 44px; padding: 10px 16px; }"
                      "QFrame#RigorConvergenceStart QPushButton#pushButtonStartConvergence:disabled {"
                      " background-color: %2; color: %4; }")
                      .arg(RigorTheme::colorHex(p.bgSurface),
                           RigorTheme::colorHex(p.borderSubtle),
                           RigorTheme::colorHex(p.textPrimary),
                           RigorTheme::colorHex(p.textMuted),
                           RigorTheme::colorHex(p.critical),
                           RigorTheme::colorHex(p.convergence)));

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    m_titleLabel = new QLabel(tr("Start Convergence"), this);
    m_titleLabel->setObjectName(QStringLiteral("labelStartTitle"));
    layout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(
        tr("Use ORU to participate in RES convergence.\n"
           "Your ORU participates during the next %1 blocks before the network determines the result.")
            .arg(Consensus::CONVERGENCE_DRAW_WINDOW),
        this);
    m_subtitleLabel->setObjectName(QStringLiteral("labelStartSubtitle"));
    m_subtitleLabel->setWordWrap(true);
    layout->addWidget(m_subtitleLabel);

    m_offlineLabel = new QLabel(this);
    m_offlineLabel->setObjectName(QStringLiteral("labelStartOffline"));
    m_offlineLabel->setWordWrap(true);
    m_offlineLabel->setVisible(false);
    layout->addWidget(m_offlineLabel);

    m_balanceTitle = new QLabel(tr("Available ORU"), this);
    m_balanceTitle->setObjectName(QStringLiteral("labelStartSection"));
    m_balanceValue = new QLabel(QStringLiteral("—"), this);
    m_balanceValue->setObjectName(QStringLiteral("labelStartMuted"));
    layout->addWidget(m_balanceTitle);
    layout->addWidget(m_balanceValue);

    m_amountTitle = new QLabel(tr("Convergence amount"), this);
    m_amountTitle->setObjectName(QStringLiteral("labelStartSection"));
    m_amountField = new BitcoinAmountField(this);
    m_amountField->setDisplayUnit(BitcoinUnit::ORU);
    m_amountField->setUnitSelectorVisible(false);
    m_amountField->SetAllowEmpty(true);
    m_amountField->SetMinValue(Consensus::MIN_CONVERGENCE_AMOUNT);
    m_amountField->setSingleStep(COIN);
    m_validationLabel = new QLabel(this);
    m_validationLabel->setObjectName(QStringLiteral("labelStartMuted"));
    m_validationLabel->setWordWrap(true);
    layout->addWidget(m_amountTitle);
    layout->addWidget(m_amountField);
    layout->addWidget(m_validationLabel);

    m_windowTitle = new QLabel(tr("Convergence period"), this);
    m_windowTitle->setObjectName(QStringLiteral("labelStartSection"));
    m_windowValue = new QLabel(
        tr("%1 blocks").arg(Consensus::CONVERGENCE_DRAW_WINDOW), this);
    m_windowValue->setObjectName(QStringLiteral("labelStartMuted"));
    layout->addWidget(m_windowTitle);
    layout->addWidget(m_windowValue);

    m_estimateTitle = new QLabel(tr("Estimated completion"), this);
    m_estimateTitle->setObjectName(QStringLiteral("labelStartSection"));
    m_estimateRemaining = new QLabel(this);
    m_estimateRemaining->setObjectName(QStringLiteral("labelStartMuted"));
    m_estimateTime = new QLabel(this);
    m_estimateTime->setObjectName(QStringLiteral("labelStartMuted"));
    layout->addWidget(m_estimateTitle);
    layout->addWidget(m_estimateRemaining);
    layout->addWidget(m_estimateTime);

    m_resultTitle = new QLabel(tr("Possible result"), this);
    m_resultTitle->setObjectName(QStringLiteral("labelStartSection"));
    const QString res_amt = QString::number(
        static_cast<double>(Consensus::RES_REWARD_AMOUNT) / Consensus::RES_COIN, 'f', 2);
    m_resultBody = new QLabel(
        tr("Result may be:\n%1 RES minted\n\nor\nNo RES minted")
            .arg(res_amt),
        this);
    m_resultBody->setObjectName(QStringLiteral("labelStartMuted"));
    m_resultBody->setWordWrap(true);
    layout->addWidget(m_resultTitle);
    layout->addWidget(m_resultBody);

    m_startButton = new QPushButton(tr("Start Convergence"), this);
    m_startButton->setObjectName(QStringLiteral("pushButtonStartConvergence"));
    m_startButton->setEnabled(false);
    layout->addWidget(m_startButton);

    connect(m_amountField, &BitcoinAmountField::valueChanged, this, &ConvergenceStartPanel::onAmountChanged);
    connect(m_startButton, &QPushButton::clicked, this, &ConvergenceStartPanel::onStartClicked);

    refresh();
}

void ConvergenceStartPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    if (isVisible()) refresh();
}

void ConvergenceStartPanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    if (isVisible()) refresh();
}

void ConvergenceStartPanel::refresh()
{
    if (!isVisible()) return;
    updateBalance();
    updateEstimate();
    updateValidation();
    updateNetworkGate();
}

void ConvergenceStartPanel::onAmountChanged()
{
    updateValidation();
    updateNetworkGate();
}

CAmount ConvergenceStartPanel::availableOru() const
{
    if (!m_walletModel) return 0;
    const auto balances = m_walletModel->getCachedBalance();
    return balances.oru_balance + balances.oru_unconfirmed_balance;
}

void ConvergenceStartPanel::updateBalance()
{
    if (!m_walletModel) {
        m_balanceValue->setText(tr("—"));
        return;
    }
    m_balanceValue->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, availableOru()));
}

void ConvergenceStartPanel::updateEstimate()
{
    const int W = Consensus::CONVERGENCE_DRAW_WINDOW;
    m_estimateRemaining->setText(tr("Remaining blocks: %1 (after confirmation)").arg(W));
    const int64_t spacing = Params().GetConsensus().nPowTargetSpacing;
    m_estimateTime->setText(tr("Estimated time: %1").arg(FormatDurationApprox(static_cast<int64_t>(W) * spacing)));
}

ConvergenceStartPanel::AmountValidity ConvergenceStartPanel::checkAmount(CAmount& amount_out) const
{
    amount_out = 0;
    if (!m_amountField) return AmountValidity::Empty;
    bool ok = false;
    amount_out = m_amountField->value(&ok);
    if (!ok || amount_out <= 0) return AmountValidity::Empty;
    if (amount_out < Consensus::MIN_CONVERGENCE_AMOUNT) return AmountValidity::BelowMinimum;
    if (m_walletModel && amount_out > availableOru()) return AmountValidity::Insufficient;
    return AmountValidity::Valid;
}

void ConvergenceStartPanel::updateValidation()
{
    // Only warn when amount is below the 1 ORU minimum — no "Valid amount" success tip.
    const ColorPalette p = RigorTheme::colors();
    CAmount amt{};
    if (checkAmount(amt) == AmountValidity::BelowMinimum) {
        m_validationLabel->setText(
            tr("Below minimum (%1).")
                .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, Consensus::MIN_CONVERGENCE_AMOUNT)));
        m_validationLabel->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px;").arg(RigorTheme::colorHex(p.critical)));
        m_validationLabel->setVisible(true);
    } else {
        m_validationLabel->clear();
        m_validationLabel->setVisible(false);
    }
}

bool ConvergenceStartPanel::networkReady(QString* warn_out) const
{
    if (!m_walletModel || !m_clientModel) {
        if (warn_out) *warn_out = tr("Open a wallet and wait for the node to become ready.");
        return false;
    }
    const int peers = m_clientModel->getNumConnections();
    const int blocks = m_clientModel->getNumBlocks();
    const int headers = m_clientModel->getHeaderTipHeight();
    const bool ibd = m_clientModel->node().isInitialBlockDownload();
    if (peers <= 0 && !Params().IsMockableChain()) {
        if (warn_out) {
            *warn_out = tr("Wallet is offline.\nConnect to the RIGOR network before starting Convergence.");
        }
        return false;
    }
    if (ibd) {
        if (warn_out) *warn_out = tr("Syncing… Wait until initial sync finishes before starting Convergence.");
        return false;
    }
    if (headers < 0 || blocks != headers) {
        if (warn_out) *warn_out = tr("Syncing… Wait until the wallet finishes catching up.");
        return false;
    }
    if (m_walletModel->wallet().privateKeysDisabled()) {
        if (warn_out) *warn_out = tr("Wallet locked for spending — unlock to start Convergence.");
        return false;
    }
    return true;
}

void ConvergenceStartPanel::updateNetworkGate()
{
    QString warn;
    const bool net_ok = networkReady(&warn);
    m_offlineLabel->setText(warn);
    m_offlineLabel->setVisible(!net_ok && m_walletModel && m_clientModel);

    CAmount amt{};
    const bool amount_ok = checkAmount(amt) == AmountValidity::Valid;
    m_startButton->setEnabled(net_ok && amount_ok);
    m_startButton->setToolTip(net_ok ? QString() : warn);
}

void ConvergenceStartPanel::onStartClicked()
{
    if (!m_walletModel || !m_clientModel) return;

    QString warn;
    if (!networkReady(&warn)) {
        QMessageBox::warning(this, tr("Convergence"), warn);
        return;
    }

    WalletModel::UnlockContext ctx(m_walletModel->requestUnlock());
    if (!ctx.isValid()) return;

    CAmount amt{};
    const AmountValidity v = checkAmount(amt);
    if (v == AmountValidity::Empty) {
        QMessageBox::warning(this, tr("Convergence"), tr("Enter a valid Convergence amount."));
        return;
    }
    if (v == AmountValidity::BelowMinimum) {
        QMessageBox::warning(
            this, tr("Convergence"),
            tr("Amount is below the minimum (%1).")
                .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, Consensus::MIN_CONVERGENCE_AMOUNT)));
        return;
    }
    if (v == AmountValidity::Insufficient) {
        QMessageBox::warning(this, tr("Convergence"), tr("Insufficient ORU available."));
        return;
    }

    const CAmount oru_avail = availableOru();
    const CAmount large_threshold = std::max(Consensus::MIN_CONVERGENCE_AMOUNT * 10, oru_avail / 2);
    if (oru_avail > 0 && amt >= large_threshold) {
        const auto r = QMessageBox::question(
            this, tr("Large Convergence amount"),
            tr("You are about to commit a large amount of ORU relative to your balance. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }

    const auto confirm = QMessageBox::question(
        this, tr("Confirm Convergence"),
        tr("Convergence uses ORU permanently. RES is not guaranteed.\n\n"
           "Amount: %1\nConvergence period: %2 blocks\n\nContinue?")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, amt))
            .arg(Consensus::CONVERGENCE_DRAW_WINDOW),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    m_startButton->setEnabled(false);
    QTimer::singleShot(2700, this, [this]() { updateNetworkGate(); });

    const auto ret = m_walletModel->sendConvergenceOru(amt);
    if (ret.status == WalletModel::OK) {
        QMessageBox::information(
            this, tr("Convergence"),
            tr("Convergence submitted. Track progress above. RES is not guaranteed."));
        m_amountField->clear();
        refresh();
        Q_EMIT convergenceSubmitted();
    } else {
        updateNetworkGate();
    }
}

} // namespace RigorWallet
