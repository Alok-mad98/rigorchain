// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_convergencepanel.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString truncateHash(const QString& hex)
{
    if (hex.isEmpty()) return QString();
    if (hex.size() <= 24) return hex;
    return hex.left(12) + QStringLiteral("\u2026") + hex.right(12);
}
} // namespace

DiagnosticsConvergencePanel::DiagnosticsConvergencePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsConvergencePanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsConvergencePanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Convergence %1").arg(AssetContext::authorityLabel(DataAuthority::Node)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_pendingLabel = new QLabel(this);
    m_completedLabel = new QLabel(this);
    m_mintLabel = new QLabel(this);
    m_lagLabel = new QLabel(this);
    form->addRow(tr("Pending convergence:"), m_pendingLabel);
    form->addRow(tr("Latest completed convergence:"), m_completedLabel);
    form->addRow(tr("Latest mint:"), m_mintLabel);
    form->addRow(tr("Mint lag (blocks):"), m_lagLabel);
    layout->addLayout(form);
}

void DiagnosticsConvergencePanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void DiagnosticsConvergencePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void DiagnosticsConvergencePanel::refresh()
{
    const ConvergenceDiagnosticsSnapshot snap =
        DiagnosticsCollector::collectConvergence(m_clientModel, m_walletModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    m_pendingLabel->setText(snap.pending ? truncateHash(snap.pending_txid) : tr("None"));
    if (snap.pending) m_pendingLabel->setToolTip(snap.pending_txid);
    m_completedLabel->setText(snap.latest_completed_txid.isEmpty()
                                  ? tr("None")
                                  : truncateHash(snap.latest_completed_txid));
    if (!snap.latest_completed_txid.isEmpty()) m_completedLabel->setToolTip(snap.latest_completed_txid);
    m_mintLabel->setText(snap.latest_mint_txid.isEmpty() ? tr("None") : truncateHash(snap.latest_mint_txid));
    if (!snap.latest_mint_txid.isEmpty()) m_mintLabel->setToolTip(snap.latest_mint_txid);
    m_lagLabel->setText(snap.mint_lag >= 0 ? QString::number(snap.mint_lag) : tr("—"));
}

} // namespace RigorWallet
