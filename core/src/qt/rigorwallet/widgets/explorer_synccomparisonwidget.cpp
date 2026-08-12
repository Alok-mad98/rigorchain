// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/explorer_synccomparisonwidget.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/explorer/explorer_status_service.h>
#include <qt/rigorwallet/rigortheme.h>

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

ExplorerSyncComparisonWidget::ExplorerSyncComparisonWidget(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorExplorerSyncComparison"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorExplorerSyncComparison { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Sync comparison"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    auto* form = new QFormLayout();
    m_nodeLabel = new QLabel(this);
    m_indexedLabel = new QLabel(this);
    m_diffLabel = new QLabel(this);
    m_diffLabel->setWordWrap(true);
    form->addRow(tr("Node height:"), m_nodeLabel);
    form->addRow(tr("Indexed height %1:").arg(AssetContext::authorityLabel(DataAuthority::Indexed)), m_indexedLabel);
    form->addRow(tr("Difference:"), m_diffLabel);
    layout->addLayout(form);

    connect(&ExplorerStatusService::instance(), &ExplorerStatusService::statusUpdated, this, [this](const ExplorerLiveStatus&) {
        refreshFromService();
    });
    refreshFromService();
}

void ExplorerSyncComparisonWidget::refreshFromService()
{
    const ExplorerLiveStatus s = ExplorerStatusService::instance().status();
    m_nodeLabel->setText(s.node_height >= 0 ? QString::number(s.node_height) : tr("—"));
    m_indexedLabel->setText(s.indexed_height >= 0 ? QString::number(s.indexed_height) : tr("—"));
    if (s.node_height >= 0 && s.indexed_height >= 0) {
        const int diff = s.node_height - s.indexed_height;
        if (diff == 0) {
            m_diffLabel->setText(tr("In sync"));
        } else if (diff > 0) {
            m_diffLabel->setText(tr("Indexer behind node by %1 blocks").arg(diff));
        } else {
            m_diffLabel->setText(tr("Indexer ahead of node by %1 blocks").arg(-diff));
        }
    } else {
        m_diffLabel->setText(tr("—"));
    }
}

} // namespace RigorWallet
