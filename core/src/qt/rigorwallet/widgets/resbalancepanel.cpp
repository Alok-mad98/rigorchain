// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/resbalancepanel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/walletmodel.h>

#include <interfaces/wallet.h>
#include <wallet/tx_semantic.h>

#include <qt/rigorwallet/explorer/explorerclient.h>

#include <QDesktopServices>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace RigorWallet {

RESBalancePanel::RESBalancePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorResBalancePanel"));
    applyTheme();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding,
                               RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding);
    layout->setSpacing(RigorTheme::spacing().unit);

    m_titleLabel = new QLabel(AssetContext::panelTitle(WalletAssetContext::RES, DataAuthority::Node), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPixelSize(RigorTheme::typography().headingPx);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(RigorTheme::spacing().sectionGap);
    grid->setVerticalSpacing(RigorTheme::spacing().unit);

    auto* confirmedTitle = new QLabel(tr("Confirmed"), this);
    auto* pendingTitle = new QLabel(tr("Pending"), this);
    auto* mintedTitle = new QLabel(tr("Historical Minted"), this);
    mintedTitle->setToolTip(tr("Lifetime RES mint credits in this wallet (node-derived transaction history)."));

    m_confirmedValue = new QLabel(QStringLiteral("—"), this);
    m_pendingValue = new QLabel(QStringLiteral("—"), this);
    m_mintedValue = new QLabel(QStringLiteral("—"), this);

    QFont valueFont = GUIUtil::fixedPitchFont(false);
    valueFont.setBold(true);
    valueFont.setPixelSize(RigorTheme::typography().balancePx);
    for (QLabel* v : {m_confirmedValue, m_pendingValue, m_mintedValue}) {
        v->setFont(valueFont);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    grid->addWidget(confirmedTitle, 0, 0);
    grid->addWidget(m_confirmedValue, 0, 1);
    grid->addWidget(pendingTitle, 1, 0);
    grid->addWidget(m_pendingValue, 1, 1);
    grid->addWidget(mintedTitle, 2, 0);
    grid->addWidget(m_mintedValue, 2, 1);
    layout->addLayout(grid);

    m_openMintExplorer = new QPushButton(tr("Open latest mint in Explorer (indexed)"), this);
    m_openMintExplorer->setEnabled(false);
    connect(m_openMintExplorer, &QPushButton::clicked, this, [this]() {
        const QString url = ExplorerClient::browseMintUrl(m_latestMintTxid);
        if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
    });
    layout->addWidget(m_openMintExplorer);
}

void RESBalancePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refreshHistoricalMinted();
}

void RESBalancePanel::applyTheme()
{
    const ColorPalette p = RigorTheme::colors();
    const Spacing s = RigorTheme::spacing();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorResBalancePanel {"
                      " background-color: %1;"
                      " border-left: %2px solid %3;"
                      " border-top: 1px solid %4;"
                      " border-right: 1px solid %4;"
                      " border-bottom: 1px solid %4;"
                      " border-radius: %5px;"
                      " color: %6;"
                      "}")
                      .arg(RigorTheme::colorHex(p.bgSurface))
                      .arg(s.assetBorderWidth)
                      .arg(RigorTheme::colorHex(p.assetRes))
                      .arg(RigorTheme::colorHex(p.borderSubtle))
                      .arg(s.borderRadiusPanel)
                      .arg(RigorTheme::colorHex(p.textPrimary)));
}

void RESBalancePanel::refreshHistoricalMinted()
{
    if (!m_walletModel) {
        m_mintedValue->setText(QStringLiteral("—"));
        m_latestMintTxid.clear();
        m_openMintExplorer->setEnabled(false);
        return;
    }
    CAmount minted_total = 0;
    int64_t latest_time = 0;
    m_latestMintTxid.clear();
    const std::set<interfaces::WalletTx> txs = m_walletModel->wallet().getWalletTxs();
    for (const interfaces::WalletTx& wtx : txs) {
        if (wtx.semantic == wallet::TxSemanticKind::ResMint) {
            minted_total += wtx.credit_res;
            if (wtx.time >= latest_time) {
                latest_time = wtx.time;
                m_latestMintTxid = QString::fromStdString(wtx.tx->GetHash().GetHex());
            }
        }
    }
    m_mintedValue->setText(BitcoinUnits::formatResWithPrivacy(minted_total, BitcoinUnits::SeparatorStyle::ALWAYS, false));
    m_openMintExplorer->setEnabled(ExplorerClient::isConfigured() && !m_latestMintTxid.isEmpty());
}

void RESBalancePanel::setBalances(const interfaces::WalletBalances& balances, bool privacy)
{
    const CAmount confirmed = balances.res_balance + balances.res_watch_only_balance;
    const CAmount pending = balances.res_locked_balance + balances.res_immature_watch_only_balance
        + balances.res_unconfirmed_balance + balances.res_unconfirmed_watch_only_balance;
    m_confirmedValue->setText(BitcoinUnits::formatResWithPrivacy(confirmed, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_pendingValue->setText(BitcoinUnits::formatResWithPrivacy(pending, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    refreshHistoricalMinted();
}

} // namespace RigorWallet
