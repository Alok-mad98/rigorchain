// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_explorerhooks.h>

#include <qt/rigorwallet/explorer/explorerclient.h>
#include <qt/rigorwallet/explorer/explorer_status_service.h>
#include <qt/rigorwallet/widgets/explorer_verificationbadge.h>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceExplorerHooks::ConvergenceExplorerHooks(QWidget* parent)
    : QFrame(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Explorer %1:").arg(QStringLiteral("(indexed)")), this));
    m_viewConvergence = new QPushButton(tr("Open convergence record"), this);
    m_viewMint = new QPushButton(tr("Open mint record"), this);
    row->addWidget(m_viewConvergence);
    row->addWidget(m_viewMint);
    row->addStretch();
    layout->addLayout(row);

    m_verificationBadge = new ExplorerVerificationBadge(this);
    layout->addWidget(m_verificationBadge);

    connect(m_viewConvergence, &QPushButton::clicked, this, [this]() {
        const QString url = ExplorerClient::browseConvergenceUrl(m_convergenceTxid);
        if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
    });
    connect(m_viewMint, &QPushButton::clicked, this, [this]() {
        const QString url = ExplorerClient::browseMintUrl(m_mintTxid);
        if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
    });

    connect(&ExplorerStatusService::instance(), &ExplorerStatusService::statusUpdated, this, [this](const ExplorerLiveStatus&) {
        refreshVerification();
    });

    setTargetTxids(QString(), QString());
}

void ConvergenceExplorerHooks::setTargetTxids(const QString& convergence_txid, const QString& mint_txid)
{
    m_convergenceTxid = convergence_txid;
    m_mintTxid = mint_txid;
    const bool configured = ExplorerClient::isConfigured();
    m_viewConvergence->setEnabled(configured && !m_convergenceTxid.isEmpty());
    m_viewMint->setEnabled(configured && !m_mintTxid.isEmpty());
    m_viewConvergence->setToolTip(configured
        ? tr("Open convergence tx %1 in explorer.").arg(m_convergenceTxid)
        : tr("Indexed data unavailable — configure RigorExplorer/ApiBaseUrl (G5-blocking)."));
    m_viewMint->setToolTip(configured
        ? tr("Open mint tx %1 in explorer.").arg(m_mintTxid)
        : tr("Indexed data unavailable — configure RigorExplorer/ApiBaseUrl (G5-blocking)."));
    refreshVerification();
}

void ConvergenceExplorerHooks::refreshVerification()
{
    if (!ExplorerClient::isConfigured() || m_convergenceTxid.isEmpty()) {
        m_verificationBadge->setVerification(ExplorerTxVerification::ExplorerUnavailable);
        return;
    }
    const ExplorerTxVerification state = ExplorerClient::verifyTransactionIndexed(m_convergenceTxid);
    const int lag = ExplorerStatusService::instance().status().indexer_lag;
    m_verificationBadge->setVerification(state, lag);
}

} // namespace RigorWallet
