// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_verificationpanel.h>

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

DiagnosticsVerificationPanel::DiagnosticsVerificationPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsVerificationPanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsVerificationPanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Verification %1").arg(AssetContext::authorityLabel(DataAuthority::Simulation)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    m_proofLabel = new QLabel(this);
    m_archiveLabel = new QLabel(this);
    m_simulationLabel = new QLabel(this);
    m_equivLabel = new QLabel(this);
    m_archiveLabel->setWordWrap(true);
    m_simulationLabel->setWordWrap(true);
    form->addRow(tr("PROOF-034 running:"), m_proofLabel);
    form->addRow(tr("Archive state:"), m_archiveLabel);
    form->addRow(tr("Simulation readiness:"), m_simulationLabel);
    form->addRow(tr("equivalence_claimed:"), m_equivLabel);
    layout->addLayout(form);
}

void DiagnosticsVerificationPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void DiagnosticsVerificationPanel::refresh()
{
    const VerificationStatusSnapshot snap = DiagnosticsCollector::collectVerification(m_clientModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    m_proofLabel->setText(snap.proof034_running ? tr("Yes") : tr("No"));
    m_archiveLabel->setText(
        snap.proof_current_height >= 0
            ? tr("%1 — height %2 / %3 (%4%)")
                  .arg(snap.archive_state)
                  .arg(snap.proof_current_height)
                  .arg(snap.proof_target_height)
                  .arg(snap.proof_progress_pct, 0, 'f', 2)
            : snap.archive_state);
    m_simulationLabel->setText(snap.simulation_readiness);
    m_equivLabel->setText(QStringLiteral("false"));
}

} // namespace RigorWallet
