// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_walletpanel.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/walletmodel.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

DiagnosticsWalletPanel::DiagnosticsWalletPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsWalletPanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsWalletPanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Wallet %1").arg(AssetContext::authorityLabel(DataAuthority::Wallet)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_loadedLabel = new QLabel(this);
    m_encryptedLabel = new QLabel(this);
    m_unlockedLabel = new QLabel(this);
    m_backupLabel = new QLabel(this);
    m_backupLabel->setWordWrap(true);
    form->addRow(tr("Wallet loaded:"), m_loadedLabel);
    form->addRow(tr("Wallet encrypted:"), m_encryptedLabel);
    form->addRow(tr("Wallet unlocked:"), m_unlockedLabel);
    form->addRow(tr("Wallet backup status:"), m_backupLabel);
    layout->addLayout(form);
}

void DiagnosticsWalletPanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void DiagnosticsWalletPanel::refresh()
{
    const WalletStatusSnapshot snap = DiagnosticsCollector::collectWallet(m_walletModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    m_loadedLabel->setText(snap.loaded ? tr("Yes") : tr("No"));
    m_encryptedLabel->setText(snap.encrypted ? tr("Yes") : tr("No"));
    m_unlockedLabel->setText(snap.unlocked ? tr("Yes") : tr("No"));
    m_backupLabel->setText(snap.backup_status);
}

} // namespace RigorWallet
