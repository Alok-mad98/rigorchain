// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/firstrunwizard.h>

#include <chainparams.h>
#include <consensus/res_issuance.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/rigorwallet/explorer/explorerclient.h>
#include <qt/rigorwallet/ui_readiness.h>
#include <qt/rigorwallet/walletidentity.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

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
    : QDialog(parent),
      m_clientModel(clientModel),
      m_walletModel(walletModel)
{
    setWindowTitle(tr("Welcome to %1").arg(WalletIdentity::brand().productName));
    setMinimumWidth(520);
    GUIUtil::ApplyRigorDialogChrome(this);
    buildPages();
}

void FirstRunWizard::buildPages()
{
    auto* layout = new QVBoxLayout(this);
    m_stack = new QStackedWidget(this);

    auto* welcome = new QWidget(this);
    auto* welcomeLayout = new QVBoxLayout(welcome);
    auto* welcomeText = new QLabel(
        tr("<p><b>Welcome to %1</b></p>"
           "<p>Your wallet connects to the <b>RIGOR</b> network.</p>"
           "<p>Initial synchronization is required before balances and Convergence status appear.</p>"
           "<p><b>ORU</b> — Proof-of-Work security and economic coordination asset.<br>"
           "<b>RES</b> — Independent scarce resource, minted only through Convergence (not mined).</p>")
            .arg(WalletIdentity::brand().productName),
        welcome);
    welcomeText->setWordWrap(true);
    welcomeLayout->addWidget(welcomeText);
    m_stack->addWidget(welcome);

    auto* checks = new QWidget(this);
    auto* checksLayout = new QVBoxLayout(checks);
    auto* syncNote = new QLabel(
        tr("<p><b>Blockchain Status</b></p>"
           "<p>Waiting for synchronization…</p>"
           "<p>Network: <b>%1</b>.</p>"
           "<p>ORU balance and RES Convergence status will appear after the chain syncs.</p>")
            .arg(WalletIdentity::networkDisplayName()),
        checks);
    syncNote->setWordWrap(true);
    checksLayout->addWidget(syncNote);
    m_nodeStatus = new QLabel(checks);
    m_explorerStatus = new QLabel(checks);
    m_explorerStatus->setVisible(false); // Explorer/indexer status is Advanced tooling, not first-run UX
    m_convergenceStatus = new QLabel(checks);
    for (QLabel* label : {m_nodeStatus, m_convergenceStatus}) {
        label->setWordWrap(true);
        checksLayout->addWidget(label);
    }
    m_stack->addWidget(checks);

    auto* ready = new QWidget(this);
    auto* readyLayout = new QVBoxLayout(ready);
    auto* readyText = new QLabel(
        tr("<p>Your wallet is ready to use after sync completes.</p>"
           "<ul>"
           "<li><b>Overview</b> — balances</li>"
           "<li><b>Send / Receive</b> — move ORU or RES</li>"
           "<li><b>Transactions</b> — history</li>"
           "<li><b>Convergence</b> — destroy ORU to mint RES at the confirming-height protocol cost</li>"
           "</ul>"
           "<p>The script is the compliance layer.</p>"),
        ready);
    readyText->setWordWrap(true);
    readyLayout->addWidget(readyText);
    m_stack->addWidget(ready);

    layout->addWidget(m_stack);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* backBtn = buttons->addButton(tr("Back"), QDialogButtonBox::ResetRole);
    QPushButton* nextBtn = buttons->addButton(tr("Next"), QDialogButtonBox::AcceptRole);
    QPushButton* finishBtn = buttons->addButton(tr("Finish"), QDialogButtonBox::ApplyRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(backBtn, &QPushButton::clicked, this, &FirstRunWizard::onBack);
    connect(nextBtn, &QPushButton::clicked, this, &FirstRunWizard::onNext);
    connect(finishBtn, &QPushButton::clicked, this, &FirstRunWizard::onFinish);
    layout->addWidget(buttons);

    showStep(0);
}

void FirstRunWizard::showStep(int index)
{
    m_step = index;
    m_stack->setCurrentIndex(index);
    if (index == 1) refreshStatusPage();
}

void FirstRunWizard::onBack()
{
    if (m_step > 0) showStep(m_step - 1);
}

void FirstRunWizard::onNext()
{
    if (m_step < m_stack->count() - 1) showStep(m_step + 1);
}

void FirstRunWizard::onFinish()
{
    markCompleted();
    accept();
}

void FirstRunWizard::refreshStatusPage()
{
    if (!m_nodeStatus) return;

    if (!m_clientModel) {
        m_nodeStatus->setText(tr("<b>Node</b>: Not connected yet — synchronization will start when the node is online."));
        m_explorerStatus->setText(tr("<b>Explorer</b>: Optional — configure later in Settings."));
        m_convergenceStatus->setText(tr("<b>RES Convergence</b>: Available after ORU acquisition and sync."));
        return;
    }

    const int peers = m_clientModel->getNumConnections();
    const int height = m_clientModel->getNumBlocks();
    const bool syncing = m_clientModel->node().isInitialBlockDownload();
    if (peers > 0 || Params().IsMockableChain()) {
        m_nodeStatus->setText(tr("<b>Node</b>: Connected — height %1%2.")
                                  .arg(height)
                                  .arg(syncing ? tr(", synchronizing") : QString()));
    } else {
        m_nodeStatus->setText(tr("<b>Node</b>: Waiting for network peers. You can continue; sync resumes when connected."));
    }

    if (ExplorerClient::isConfigured()) {
        const auto status = ExplorerClient::fetchLiveStatus(m_clientModel);
        if (status.connected) {
            m_explorerStatus->setText(tr("<b>Explorer</b>: Connected — indexed height %1.").arg(status.indexed_height));
        } else {
            m_explorerStatus->setText(tr("<b>Explorer</b>: Configured but unreachable. Check the URL in Settings."));
        }
    } else {
        m_explorerStatus->setText(tr("<b>Explorer</b>: Not configured (optional)."));
    }

    const QString minAmt = BitcoinUnits::format(BitcoinUnit::ORU, Consensus::ResMinMintBurnAtoms(0));
    m_convergenceStatus->setText(
        tr("<b>RES Convergence</b>: Available after ORU acquisition. Minimum to mint 1 RES atom is %1 ORU at genesis, then follows ResMinMintBurnAtoms(h).")
            .arg(minAmt));
}

} // namespace RigorWallet
