// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/diagnosticspage.h>

#include <chainparams.h>
#include <interfaces/convergence_block_result.h>
#include <interfaces/wallet.h>

#include <qt/clientmodel.h> // SyncType, SynchronizationState
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/diagnostics/diagnostics_export.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/widgets/diagnostics_nodepanel.h>
#include <qt/rigorwallet/widgets/diagnostics_walletpanel.h>
#include <qt/rigorwallet/widgets/diagnostics_explorerpanel.h>
#include <qt/rigorwallet/widgets/diagnostics_convergencepanel.h>
#include <qt/rigorwallet/widgets/diagnostics_verificationpanel.h>
#include <qt/rigorwallet/widgets/explorer_healthwidget.h>
#include <qt/rigorwallet/widgets/explorer_synccomparisonwidget.h>
#include <qt/rigorwallet/widgets/explorer_observerhooks.h>
#include <qt/rigorwallet/widgets/release_gate_panel.h>
#include <qt/rigorwallet/widgets/warningbanner.h>
#include <qt/rigorwallet/ui_readiness.h>

#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

DiagnosticsPage::DiagnosticsPage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent),
      m_platform_style(platformStyle)
{
    setObjectName(QStringLiteral("DiagnosticsPageRoot"));

    auto* outer = new QVBoxLayout(this);
    auto* title = new QLabel(tr("System Diagnostics"), this);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 600;"));
    outer->addWidget(title);
    auto* subtitle = new QLabel(
        tr("Developer and protocol tooling. Convergence Details preserves fields removed from the user Convergence page."),
        this);
    subtitle->setWordWrap(true);
    outer->addWidget(subtitle);

    m_warningBanner = new RigorWallet::WarningBanner(this);
    outer->addWidget(m_warningBanner);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);

    m_nodePanel = new RigorWallet::DiagnosticsNodePanel(content);
    m_walletPanel = new RigorWallet::DiagnosticsWalletPanel(content);
    m_explorerPanel = new RigorWallet::DiagnosticsExplorerPanel(content);
    m_explorerHealth = new RigorWallet::ExplorerHealthWidget(content);
    m_explorerSync = new RigorWallet::ExplorerSyncComparisonWidget(content);
    m_convergencePanel = new RigorWallet::DiagnosticsConvergencePanel(content);
    m_verificationPanel = new RigorWallet::DiagnosticsVerificationPanel(content);
    m_releaseGatePanel = new RigorWallet::ReleaseGatePanel(content);
    m_explorerObserverHooks = new RigorWallet::ExplorerObserverHooks(content);

    layout->addWidget(m_nodePanel);
    layout->addWidget(m_walletPanel);
    layout->addWidget(m_explorerPanel);
    layout->addWidget(m_explorerHealth);
    layout->addWidget(m_explorerSync);
    layout->addWidget(m_convergencePanel);
    layout->addWidget(m_verificationPanel);
    layout->addWidget(m_releaseGatePanel);
    m_explorerObserverHooks->setVisible(false);
    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto* exportButton = new QPushButton(tr("Export Diagnostics Bundle…"), this);
    if (platformStyle && platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/export")));
    }
    outer->addWidget(exportButton);

    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select diagnostics export directory"), QString(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (dir.isEmpty()) return;
        QString error;
        if (!RigorWallet::DiagnosticsExport::exportBundle(dir, clientModel, walletModel, &error)) {
            QMessageBox::warning(this, tr("Export failed"), error.isEmpty() ? tr("Could not write files.") : error);
            return;
        }
        QMessageBox::information(this, tr("Export complete"),
            tr("Diagnostics bundle written to:\n%1").arg(dir));
    });

    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(500);
    connect(m_refreshDebounce, &QTimer::timeout, this, &DiagnosticsPage::refreshAll);
}

void DiagnosticsPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleRefresh();
}

void DiagnosticsPage::scheduleRefresh()
{
    if (!isVisible()) return;
    if (!m_refreshDebounce) {
        refreshAll();
        return;
    }
    m_refreshDebounce->start();
}

void DiagnosticsPage::setClientModel(ClientModel* model)
{
    clientModel = model;
    if (m_nodePanel) m_nodePanel->setClientModel(model);
    if (m_explorerPanel) m_explorerPanel->setClientModel(model);
    if (m_convergencePanel) m_convergencePanel->setClientModel(model);
    if (m_verificationPanel) m_verificationPanel->setClientModel(model);
    if (m_releaseGatePanel) m_releaseGatePanel->setClientModel(model);

    if (clientModel) {
        connect(clientModel, &ClientModel::numBlocksChanged, this, [this](int, const QDateTime&, double, SyncType, SynchronizationState) {
            scheduleRefresh();
        });
        connect(clientModel, &ClientModel::numConnectionsChanged, this, [this](int) {
            scheduleRefresh();
        });
    }
    scheduleRefresh();
}

void DiagnosticsPage::setWalletModel(WalletModel* model)
{
    walletModel = model;
    if (m_walletPanel) m_walletPanel->setWalletModel(model);
    if (m_convergencePanel) m_convergencePanel->setWalletModel(model);
    if (m_releaseGatePanel) m_releaseGatePanel->setWalletModel(model);

    if (walletModel) {
        connect(walletModel, &WalletModel::encryptionStatusChanged, this, [this]() { scheduleRefresh(); });
        connect(walletModel, &WalletModel::walletStateRevisionChanged, this, [this](quint64) { scheduleRefresh(); });
    }
    scheduleRefresh();
}

void DiagnosticsPage::refreshAll()
{
    // Advanced Diagnostics is rarely open; skip RPC fan-out when the page is hidden.
    if (!isVisible()) return;
    if (m_nodePanel) m_nodePanel->refresh();
    if (m_walletPanel) m_walletPanel->refresh();
    if (m_explorerPanel) m_explorerPanel->refresh();
    if (m_convergencePanel) m_convergencePanel->refresh();
    if (m_verificationPanel) m_verificationPanel->refresh();
    if (m_releaseGatePanel) m_releaseGatePanel->refresh();
    updateWarnings();
}

void DiagnosticsPage::updateWarnings()
{
    if (!m_warningBanner) return;

    if (RigorWallet::FINAL_FREEZE_MODE) {
        m_warningBanner->showBanner(
            RigorWallet::WarningLevel::Info,
            tr("System is frozen for Testnet Candidate evaluation"));
        return;
    }

    QString msg;
    RigorWallet::WarningLevel level = RigorWallet::WarningLevel::Info;

    if (!clientModel || !walletModel) {
        m_warningBanner->hideBanner();
        return;
    }

    const RigorWallet::NodeStatusSnapshot node = RigorWallet::DiagnosticsCollector::collectNode(clientModel);
    const RigorWallet::ExplorerStatusSnapshot explorer = RigorWallet::DiagnosticsCollector::collectExplorer(clientModel);
    const RigorWallet::VerificationStatusSnapshot verification =
        RigorWallet::DiagnosticsCollector::collectVerification(clientModel);

    if (node.peers <= 0 && !Params().IsMockableChain()) {
        msg = tr("Node offline — connect to the network.");
        level = RigorWallet::WarningLevel::Critical;
    } else if (explorer.api_base_url.isEmpty()) {
        msg = tr("Explorer not configured — add an explorer URL in Settings to enable indexed verification.");
        level = RigorWallet::WarningLevel::Verification;
    } else if (explorer.connected && explorer.indexer_lag > 10) {
        msg = tr("Explorer lag — indexer is %1 blocks behind the node.").arg(explorer.indexer_lag);
        level = RigorWallet::WarningLevel::Warning;
    } else if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        msg = tr("Wallet locked for spending.");
        level = RigorWallet::WarningLevel::Warning;
    } else {
        const auto rows = RigorWallet::ConvergenceWalletData::buildHistory(walletModel);
        if (const auto* pending = RigorWallet::ConvergenceWalletData::pendingRow(rows)) {
            msg = tr("Convergence pending: %1").arg(pending->txid.left(16) + QStringLiteral("…"));
            level = RigorWallet::WarningLevel::Verification;
        } else {
            interfaces::ConvergenceUiSnapshot snap;
            if (walletModel->wallet().tryGetConvergenceUiSnapshot(snap) && snap.has_convergence) {
                const QString st = QString::fromStdString(snap.status_text);
                if (st.contains(QLatin1String("Locked"), Qt::CaseInsensitive)) {
                    msg = tr("Mint delayed — RES locked until maturity.");
                    level = RigorWallet::WarningLevel::Warning;
                }
            }
        }
    }

    if (msg.isEmpty() && verification.proof_progress_pct < 100.0) {
        msg = tr("Archive verification in progress (%1%). See Diagnostics for details.")
                  .arg(verification.proof_progress_pct, 0, 'f', 0);
        level = RigorWallet::WarningLevel::Verification;
    }

    if (msg.isEmpty()) {
        m_warningBanner->hideBanner();
    } else {
        m_warningBanner->showBanner(level, msg);
    }
}
