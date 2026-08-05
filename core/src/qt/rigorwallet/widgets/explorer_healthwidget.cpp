// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/explorer_healthwidget.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/explorer/explorer_status_service.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

ExplorerHealthWidget::ExplorerHealthWidget(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorExplorerHealth"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorExplorerHealth { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Explorer Health %1").arg(AssetContext::authorityLabel(DataAuthority::Indexed)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_reachableLabel = new QLabel(this);
    m_latencyLabel = new QLabel(this);
    m_heightLabel = new QLabel(this);
    m_lagLabel = new QLabel(this);
    form->addRow(tr("Explorer reachable:"), m_reachableLabel);
    form->addRow(tr("Response latency:"), m_latencyLabel);
    form->addRow(tr("Indexed height:"), m_heightLabel);
    form->addRow(tr("Lag:"), m_lagLabel);
    layout->addLayout(form);

    connect(&ExplorerStatusService::instance(), &ExplorerStatusService::statusUpdated, this, [this](const ExplorerLiveStatus&) {
        refreshFromService();
    });
    refreshFromService();
}

void ExplorerHealthWidget::refreshFromService()
{
    const ExplorerLiveStatus s = ExplorerStatusService::instance().status();
    StatusBadgeColor color = StatusBadgeColor::Gray;
    if (!s.configured) {
        m_badge->setStatus(StatusBadgeColor::Gray, tr("Explorer Disconnected (G5-blocking)"));
    } else if (s.reachable && s.indexer_lag > 10) {
        color = StatusBadgeColor::Yellow;
        m_badge->setStatus(color, tr("Lag"));
    } else if (s.connected) {
        color = StatusBadgeColor::Green;
        m_badge->setStatus(color, tr("OK"));
    } else {
        color = StatusBadgeColor::Red;
        m_badge->setStatus(color, tr("Offline"));
    }

    m_reachableLabel->setText(s.reachable ? tr("Yes") : tr("No"));
    m_latencyLabel->setText(s.response_latency_ms >= 0 ? tr("%1 ms").arg(s.response_latency_ms) : tr("—"));
    m_heightLabel->setText(s.indexed_height >= 0 ? QString::number(s.indexed_height) : tr("—"));
    m_lagLabel->setText(s.indexer_lag >= 0 ? QString::number(s.indexer_lag) : tr("—"));
}

} // namespace RigorWallet
