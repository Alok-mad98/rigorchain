// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_nodepanel.h>

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

namespace {
QString truncateHash(const QString& hex)
{
    if (hex.size() <= 24) return hex;
    return hex.left(12) + QStringLiteral("\u2026") + hex.right(12);
}
} // namespace

DiagnosticsNodePanel::DiagnosticsNodePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsNodePanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsNodePanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Node"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_heightLabel = new QLabel(this);
    m_headersLabel = new QLabel(this);
    m_peersLabel = new QLabel(this);
    m_syncLabel = new QLabel(this);
    m_hashLabel = new QLabel(this);
    m_hashLabel->setWordWrap(true);
    form->addRow(tr("Block height:"), m_heightLabel);
    form->addRow(tr("Headers:"), m_headersLabel);
    form->addRow(tr("Peers:"), m_peersLabel);
    form->addRow(tr("Sync progress:"), m_syncLabel);
    form->addRow(tr("Best block hash:"), m_hashLabel);
    layout->addLayout(form);
}

void DiagnosticsNodePanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void DiagnosticsNodePanel::refresh()
{
    const NodeStatusSnapshot snap = DiagnosticsCollector::collectNode(m_clientModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    if (!m_clientModel) {
        m_heightLabel->setText(tr("—"));
        m_headersLabel->setText(tr("—"));
        m_peersLabel->setText(tr("—"));
        m_syncLabel->setText(tr("—"));
        m_hashLabel->setText(tr("—"));
        return;
    }
    m_heightLabel->setText(QString::number(snap.block_height));
    m_headersLabel->setText(QString::number(snap.headers));
    m_peersLabel->setText(QString::number(snap.peers));
    m_syncLabel->setText(QStringLiteral("%1%").arg(snap.sync_progress * 100.0, 0, 'f', 2));
    m_hashLabel->setText(snap.best_block_hash.isEmpty() ? tr("—") : truncateHash(snap.best_block_hash));
    if (!snap.best_block_hash.isEmpty()) m_hashLabel->setToolTip(snap.best_block_hash);
}

} // namespace RigorWallet
