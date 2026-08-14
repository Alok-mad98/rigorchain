// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_lifecyclepanel.h>

#include <qt/clientmodel.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/walletmodel.h>

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <wallet/convergence_window_ui.h>

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceLifecyclePanel::ConvergenceLifecyclePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceLifecycle"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorConvergenceLifecycle {"
                      " background: %1; border-left: 4px solid %2;"
                      " border: 1px solid %3; border-radius: 6px; padding: 10px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface),
                           RigorTheme::colorHex(p.convergence),
                           RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(tr("My Convergence"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px;"));
    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet(QStringLiteral("color: %1;").arg(RigorTheme::colorHex(p.textSecondary)));

    m_windowLabel = new QLabel(this);
    m_windowLabel->setWordWrap(true);

    auto* progressTitle = new QLabel(tr("Progress"), this);
    progressTitle->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 4px;"));
    // Start / current / finalization block numbers stay in System Diagnostics only.
    m_startLabel = new QLabel(this);
    m_startLabel->setVisible(false);
    m_currentLabel = new QLabel(this);
    m_currentLabel->setVisible(false);
    m_finalLabel = new QLabel(this);
    m_finalLabel->setVisible(false);
    m_remainingLabel = new QLabel(this);
    m_remainingLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, Consensus::CONVERGENCE_DRAW_WINDOW);
    m_progress->setTextVisible(true);
    m_progress->setFormat(tr("%v / %m blocks completed"));

    auto* resultTitle = new QLabel(tr("Result"), this);
    resultTitle->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 4px;"));
    m_resultLabel = new QLabel(this);
    m_resultLabel->setWordWrap(true);

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_detailLabel);
    layout->addWidget(m_windowLabel);
    layout->addWidget(progressTitle);
    layout->addWidget(m_remainingLabel);
    layout->addWidget(m_progress);
    layout->addWidget(resultTitle);
    layout->addWidget(m_resultLabel);

    refresh();
}

void ConvergenceLifecyclePanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    if (isVisible()) refresh();
}

void ConvergenceLifecyclePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    if (isVisible()) refresh();
}

void ConvergenceLifecyclePanel::refresh()
{
    if (!isVisible()) return;
    const int period = Consensus::CONVERGENCE_DRAW_WINDOW;

    m_windowLabel->setText(
        tr("Convergence period:\n%1 blocks\n\n"
           "Your ORU participates during the next %1 blocks before the network determines the result.")
            .arg(period));

    if (!m_walletModel) {
        m_statusLabel->setText(tr("Status: No active Convergence"));
        m_detailLabel->setText(tr("Open a wallet to track Convergence."));
        m_remainingLabel->setText(tr("Remaining blocks: —"));
        m_resultLabel->setText(tr("—"));
        m_progress->setValue(0);
        return;
    }

    const int tip = m_clientModel ? m_clientModel->getNumBlocks() : -1;
    const auto rows = ConvergenceWalletData::buildHistory(m_walletModel, tip);
    const ConvergenceHistoryRow* focus = nullptr;
    for (const ConvergenceHistoryRow& r : rows) {
        if (r.lifecycle == wallet::ConvergenceUiLifecycle::Submitted ||
            r.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow ||
            r.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending) {
            focus = &r;
            break;
        }
    }
    if (!focus && !rows.empty()) {
        focus = &rows.front();
    }

    if (!focus) {
        m_statusLabel->setText(tr("Status: No active Convergence"));
        m_detailLabel->setText(tr("Start a Convergence below to participate."));
        m_remainingLabel->setText(tr("Remaining blocks: —"));
        m_resultLabel->setText(tr("—"));
        m_progress->setValue(0);
        return;
    }

    const wallet::ConvergenceUiWindowSnapshot& w = focus->window;
    m_statusLabel->setText(tr("Status: %1").arg(QString::fromUtf8(wallet::ConvergenceUiLifecycleLabel(w.lifecycle))));
    m_detailLabel->setText(QString::fromStdString(wallet::ConvergenceUiLifecycleDetail(
        w.lifecycle, w.lifecycle == wallet::ConvergenceUiLifecycle::CompletedAllocated)));

    if (w.start_block >= 0) {
        m_progress->setValue(w.elapsed_in_window);
        m_progress->setFormat(tr("%1 / %2 blocks completed").arg(w.elapsed_in_window).arg(w.W));
    } else {
        m_progress->setValue(0);
        m_progress->setFormat(tr("0 / %1 blocks completed").arg(period));
    }

    if (w.lifecycle == wallet::ConvergenceUiLifecycle::Submitted) {
        m_remainingLabel->setText(tr("Remaining blocks: waiting for confirmation"));
    } else if (w.lifecycle == wallet::ConvergenceUiLifecycle::CompletedAllocated ||
               w.lifecycle == wallet::ConvergenceUiLifecycle::CompletedNoAllocation) {
        m_remainingLabel->setText(tr("Remaining blocks: 0"));
    } else {
        m_remainingLabel->setText(tr("Remaining blocks: %1").arg(w.remaining_blocks));
    }

    if (w.lifecycle == wallet::ConvergenceUiLifecycle::CompletedAllocated) {
        QString res_disp;
        if (focus->res_amount > 0) {
            res_disp = QString::number(static_cast<double>(focus->res_amount) / Consensus::RES_COIN, 'f', 2) +
                       QStringLiteral(" RES minted");
        } else {
            res_disp = QStringLiteral("%1.00 RES minted").arg(Consensus::RES_REWARD_AMOUNT / Consensus::RES_COIN);
        }
        m_resultLabel->setText(res_disp);
    } else if (w.lifecycle == wallet::ConvergenceUiLifecycle::CompletedNoAllocation) {
        m_resultLabel->setText(tr("No RES minted"));
    } else if (w.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending) {
        m_resultLabel->setText(tr("Waiting for network result"));
    } else if (w.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow) {
        m_resultLabel->setText(tr("Waiting for confirmation"));
    } else {
        m_resultLabel->setText(tr("Waiting for confirmation"));
    }
}

} // namespace RigorWallet
