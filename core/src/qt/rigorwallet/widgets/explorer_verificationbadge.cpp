// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/explorer_verificationbadge.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>

#include <QHBoxLayout>
#include <QLabel>

namespace RigorWallet {

ExplorerVerificationBadge::ExplorerVerificationBadge(QWidget* parent)
    : QFrame(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_badge = new StatusBadge(this);
    m_detailLabel = new QLabel(this);
    m_detailLabel->setStyleSheet(QStringLiteral("color: #8B9CB3; font-size: 11px;"));
    layout->addWidget(m_badge);
    layout->addWidget(m_detailLabel);
    layout->addStretch();
    setVerification(ExplorerTxVerification::Unknown);
}

void ExplorerVerificationBadge::setVerification(ExplorerTxVerification state, int indexer_lag)
{
    StatusBadgeColor color = StatusBadgeColor::Gray;
    QString text = ExplorerClient::verificationLabel(state);
    switch (state) {
    case ExplorerTxVerification::VerifiedIndexed:
        color = StatusBadgeColor::Green;
        break;
    case ExplorerTxVerification::NotIndexed:
        color = StatusBadgeColor::Red;
        break;
    case ExplorerTxVerification::ExplorerUnavailable:
        color = StatusBadgeColor::Gray;
        break;
    case ExplorerTxVerification::Unknown:
        break;
    }
    if (indexer_lag > 10 && state == ExplorerTxVerification::VerifiedIndexed) {
        color = StatusBadgeColor::Yellow;
        text = tr("Indexer lagging");
    }
    m_badge->setStatus(color, text);
    m_detailLabel->setText(AssetContext::authorityLabel(DataAuthority::Indexed));
}

void ExplorerVerificationBadge::setFromLag(int indexer_lag)
{
    if (indexer_lag < 0) {
        setVerification(ExplorerTxVerification::Unknown);
        return;
    }
    if (indexer_lag > 10) {
        setVerification(ExplorerTxVerification::VerifiedIndexed, indexer_lag);
    } else if (indexer_lag >= 0) {
        setVerification(ExplorerTxVerification::VerifiedIndexed, indexer_lag);
    }
}

} // namespace RigorWallet
