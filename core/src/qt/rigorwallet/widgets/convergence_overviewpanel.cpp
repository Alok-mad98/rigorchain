// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_overviewpanel.h>

#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/walletmodel.h>

#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString TruncateMiddleHex(const QString& hex, int head = 12, int tail = 12)
{
    if (hex.size() <= head + tail + 1) return hex;
    return hex.left(head) + QStringLiteral("\u2026") + hex.right(tail);
}
} // namespace

ConvergenceOverviewPanel::ConvergenceOverviewPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceOverview"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorConvergenceOverview { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Convergence Overview %1").arg(AssetContext::authorityLabel(DataAuthority::Node)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    m_activityLabel = new QLabel(this);
    m_historicalLabel = new QLabel(this);
    m_latestLabel = new QLabel(this);
    m_pendingLabel = new QLabel(this);
    for (QLabel* l : {m_activityLabel, m_historicalLabel, m_latestLabel, m_pendingLabel}) {
        l->setWordWrap(true);
        layout->addWidget(l);
    }
}

void ConvergenceOverviewPanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void ConvergenceOverviewPanel::refresh()
{
    if (!m_walletModel) {
        m_activityLabel->setText(tr("Convergence activity: —"));
        m_historicalLabel->setText(tr("Historical count: —"));
        m_latestLabel->setText(tr("Latest convergence: —"));
        m_pendingLabel->setText(tr("Pending convergence: —"));
        return;
    }

    const auto rows = ConvergenceWalletData::buildHistory(m_walletModel);
    const int pending = ConvergenceWalletData::countByStatus(rows, ConvergenceUserStatus::Pending);
    const int historical = ConvergenceWalletData::historicalCount(rows);
    const ConvergenceHistoryRow* latest = ConvergenceWalletData::latestRow(rows);
    const ConvergenceHistoryRow* pending_row = ConvergenceWalletData::pendingRow(rows);

    m_activityLabel->setText(tr("Convergence activity: %1 total in wallet history.").arg(historical));
    m_historicalLabel->setText(tr("Historical convergence count: %1").arg(historical));
    if (latest) {
        m_latestLabel->setText(tr("Latest convergence: %1 (%2)")
                                   .arg(TruncateMiddleHex(latest->txid), latest->status_label));
    } else {
        m_latestLabel->setText(tr("Latest convergence: none"));
    }
    if (pending_row) {
        m_pendingLabel->setText(tr("Pending convergence: %1 (%2 pending total)")
                                    .arg(TruncateMiddleHex(pending_row->txid))
                                    .arg(pending));
    } else {
        m_pendingLabel->setText(tr("Pending convergence: none"));
    }
}

} // namespace RigorWallet
