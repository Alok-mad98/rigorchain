// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/release_gate_panel.h>

#include <release/release_gate_engine.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/ui_readiness.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace RigorWallet {

ReleaseGatePanel::ReleaseGatePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorReleaseGatePanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorReleaseGatePanel {"
                      " background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    m_titleLabel = new QLabel(
        tr("Release Gate %1").arg(AssetContext::authorityLabel(DataAuthority::Simulation)), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_titleLabel);

    m_gateLabel = new QLabel(this);
    m_testnetLabel = new QLabel(this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_blockersLabel = new QLabel(this);
    m_blockersLabel->setWordWrap(true);
    m_nextActionLabel = new QLabel(this);
    m_nextActionLabel->setWordWrap(true);

    for (QLabel* label : {m_gateLabel, m_testnetLabel, m_blockersLabel, m_nextActionLabel}) {
        layout->addWidget(label);
    }
    layout->addWidget(m_progressBar);
}

void ReleaseGatePanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void ReleaseGatePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void ReleaseGatePanel::refresh()
{
    const ReleaseGate::Evaluation eval = ReleaseGate::Engine::evaluate(m_clientModel, m_walletModel);

    m_gateLabel->setText(tr("Current gate: %1").arg(ReleaseGate::Engine::levelLabel(eval.current_level)));
    if (FINAL_FREEZE_MODE) {
        m_titleLabel->setText(
            tr("Release Gate %1 — %2")
                .arg(AssetContext::authorityLabel(DataAuthority::Simulation),
                     tr("Testnet Candidate freeze")));
    }
    m_testnetLabel->setText(
        eval.testnet_release_ready
            ? tr("Testnet release: YES")
            : tr("Testnet release: NO"));
    m_progressBar->setValue(eval.progress_percent);
    m_progressBar->setFormat(tr("%1%").arg(eval.progress_percent));

    if (eval.blockers.isEmpty()) {
        m_blockersLabel->setText(tr("Blocking conditions: none"));
    } else {
        QStringList lines;
        for (const ReleaseGate::Blocker& b : eval.blockers) {
            lines << QStringLiteral("[%1] %2 — %3").arg(b.code, b.gate_id, b.message);
        }
        m_blockersLabel->setText(tr("Blocking conditions:\n%1").arg(lines.join(QStringLiteral("\n"))));
    }
    m_nextActionLabel->setText(tr("Next required action: %1").arg(eval.next_required_action));

    exportSnapshots();
}

void ReleaseGatePanel::exportSnapshots()
{
    const ReleaseGate::ArtifactSignals artifacts = ReleaseGate::Engine::loadArtifacts();
    const ReleaseGate::Evaluation eval = ReleaseGate::Engine::evaluate(m_clientModel, m_walletModel);

    QStringList roots = ReleaseGate::Engine::searchRoots();
    QString output_dir;
    for (const QString& root : roots) {
        const QString candidate = QDir(root).absoluteFilePath(QStringLiteral(".runtime/release_gate"));
        if (QDir(root).exists()) {
            output_dir = candidate;
            break;
        }
    }
    if (output_dir.isEmpty()) {
        output_dir = QDir::home().absoluteFilePath(QStringLiteral(".runtime/release_gate"));
    }
    ReleaseGate::Engine::writeSnapshots(eval, artifacts, output_dir, nullptr);
}

} // namespace RigorWallet
