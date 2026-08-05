// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/firstrunwizard.h>

#include <chainparams.h>
#include <consensus/convergenceres.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/rigorwallet/explorer/explorerclient.h>
#include <qt/rigorwallet/ui_readiness.h>
#include <qt/rigorwallet/walletidentity.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>

#include <QCoreApplication>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>
#include <QWizardPage>

namespace RigorWallet {

namespace {
constexpr auto kFirstRunKey = "firstrun/completed_v1";
} // namespace

bool FirstRunWizard::shouldShow()
{
    if (!UI_RELEASE_CANDIDATE_READY) return false;
    return !QSettings().value(QLatin1String(kFirstRunKey), false).toBool();
}

void FirstRunWizard::markCompleted()
{
    QSettings().setValue(QLatin1String(kFirstRunKey), true);
}

FirstRunWizard::FirstRunWizard(ClientModel* clientModel, WalletModel* walletModel, QWidget* parent)
    : QWizard(parent),
      m_clientModel(clientModel),
      m_walletModel(walletModel)
{
    setWindowTitle(tr("Welcome to %1").arg(WalletIdentity::brand().productName));
    setWizardStyle(QWizard::ModernStyle);
    setPixmap(QWizard::LogoPixmap, QPixmap());
    setOption(QWizard::NoBackButtonOnStartPage, true);
    buildPages();
}

void FirstRunWizard::buildPages()
{
    auto* welcome = new QWizardPage(this);
    welcome->setTitle(tr("Welcome"));
    auto* welcomeLayout = new QVBoxLayout(welcome);
    auto* welcomeText = new QLabel(
        tr("<p><b>%1</b> helps you hold ORU and RES, send payments, and participate in convergence.</p>"
           "<p>This short setup checks that your node and optional explorer are ready.</p>")
            .arg(WalletIdentity::brand().productName),
        welcome);
    welcomeText->setWordWrap(true);
    welcomeLayout->addWidget(welcomeText);
    addPage(welcome);

    auto* checks = new QWizardPage(this);
    checks->setTitle(tr("System readiness"));
    auto* checksLayout = new QVBoxLayout(checks);
    m_nodeStatus = new QLabel(checks);
    m_explorerStatus = new QLabel(checks);
    m_convergenceStatus = new QLabel(checks);
    for (QLabel* label : {m_nodeStatus, m_explorerStatus, m_convergenceStatus}) {
        label->setWordWrap(true);
        checksLayout->addWidget(label);
    }
    connect(this, &QWizard::currentIdChanged, this, [this](int) { refreshStatusPage(); });
    addPage(checks);

    auto* ready = new QWizardPage(this);
    ready->setTitle(tr("Ready"));
    auto* readyLayout = new QVBoxLayout(ready);
    auto* readyText = new QLabel(
        tr("<p>Your wallet is ready to use.</p>"
           "<ul>"
           "<li><b>Receive</b> — get ORU from others</li>"
           "<li><b>Convergence</b> — commit ORU toward protocol selection</li>"
           "<li><b>History</b> — review transactions and mints</li>"
           "</ul>"
           "<p>Advanced system details are available under <b>Diagnostics</b>.</p>"),
        ready);
    readyText->setWordWrap(true);
    readyLayout->addWidget(readyText);
    addPage(ready);

    connect(this, &QWizard::accepted, this, &FirstRunWizard::markCompleted);
}

void FirstRunWizard::refreshStatusPage()
{
    if (!m_nodeStatus) return;

    if (!m_clientModel) {
        m_nodeStatus->setText(tr("<b>Node</b>: Not connected."));
        m_explorerStatus->setText(tr("<b>Explorer</b>: Configure later in Settings."));
        m_convergenceStatus->setText(tr("<b>Convergence</b>: Unavailable until the node is connected."));
        return;
    }

    const int peers = m_clientModel->getNumConnections();
    const int height = m_clientModel->getNumBlocks();
    const bool syncing = m_clientModel->node().isInitialBlockDownload();
    if (peers > 0 || Params().IsMockableChain()) {
        m_nodeStatus->setText(tr("<b>Node</b>: Connected — height %1%2.")
                                  .arg(height)
                                  .arg(syncing ? tr(", syncing") : QString()));
    } else {
        m_nodeStatus->setText(tr("<b>Node</b>: Waiting for network peers. You can continue; sync will resume when connected."));
    }

    if (ExplorerClient::isConfigured()) {
        const auto status = ExplorerClient::fetchLiveStatus(m_clientModel);
        if (status.connected) {
            m_explorerStatus->setText(tr("<b>Explorer</b>: Connected — indexed height %1.").arg(status.indexed_height));
        } else {
            m_explorerStatus->setText(tr("<b>Explorer</b>: Configured but unreachable. Check the URL in Settings."));
        }
    } else {
        m_explorerStatus->setText(tr("<b>Explorer</b>: Not configured (optional). Add a base URL in Settings to enable indexed verification."));
    }

    const QString minAmt = BitcoinUnits::format(BitcoinUnit::ORU, Consensus::MIN_CONVERGENCE_AMOUNT);
    m_convergenceStatus->setText(tr("<b>Convergence</b>: Minimum amount %1 ORU. RES mint is not guaranteed.").arg(minAmt));
}

} // namespace RigorWallet
