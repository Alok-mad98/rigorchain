// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_explorerpanel.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/clientmodel.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

DiagnosticsExplorerPanel::DiagnosticsExplorerPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsExplorerPanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsExplorerPanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Explorer %1").arg(AssetContext::authorityLabel(DataAuthority::Indexed)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_connectionLabel = new QLabel(this);
    m_lagLabel = new QLabel(this);
    m_heightLabel = new QLabel(this);
    form->addRow(tr("Connection:"), m_connectionLabel);
    form->addRow(tr("Indexer lag:"), m_lagLabel);
    form->addRow(tr("Latest indexed height:"), m_heightLabel);
    layout->addLayout(form);
}

void DiagnosticsExplorerPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void DiagnosticsExplorerPanel::refresh()
{
    const ExplorerStatusSnapshot snap = DiagnosticsCollector::collectExplorer(m_clientModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    if (snap.api_base_url.isEmpty()) {
        m_connectionLabel->setText(tr("Explorer Disconnected (G5-blocking) — indexed data unavailable"));
    } else {
        m_connectionLabel->setText(snap.connected ? tr("Connected") : tr("Disconnected"));
    }
    m_lagLabel->setText(snap.indexer_lag >= 0 ? QString::number(snap.indexer_lag) : tr("—"));
    m_heightLabel->setText(snap.latest_indexed_height >= 0 ? QString::number(snap.latest_indexed_height) : tr("—"));
}

} // namespace RigorWallet
