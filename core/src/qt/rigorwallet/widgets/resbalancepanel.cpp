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

#include <QFont>
#include <QGridLayout>
#include <QLabel>
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
    auto* totalTitle = new QLabel(tr("Total"), this);
    totalTitle->setToolTip(tr("Confirmed + Pending (current wallet RES balance)."));

    m_confirmedValue = new QLabel(QStringLiteral("—"), this);
    m_pendingValue = new QLabel(QStringLiteral("—"), this);
    m_totalValue = new QLabel(QStringLiteral("—"), this);

    QFont valueFont = GUIUtil::fixedPitchFont(false);
    valueFont.setBold(true);
    valueFont.setPixelSize(RigorTheme::typography().balancePx);
    for (QLabel* v : {m_confirmedValue, m_pendingValue, m_totalValue}) {
        v->setFont(valueFont);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    grid->addWidget(confirmedTitle, 0, 0);
    grid->addWidget(m_confirmedValue, 0, 1);
    grid->addWidget(pendingTitle, 1, 0);
    grid->addWidget(m_pendingValue, 1, 1);
    grid->addWidget(totalTitle, 2, 0);
    grid->addWidget(m_totalValue, 2, 1);
    layout->addLayout(grid);
}

void RESBalancePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
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

void RESBalancePanel::setBalances(const interfaces::WalletBalances& balances, bool privacy)
{
    const CAmount confirmed = balances.res_balance + balances.res_watch_only_balance;
    const CAmount pending = balances.res_locked_balance + balances.res_immature_watch_only_balance
        + balances.res_unconfirmed_balance + balances.res_unconfirmed_watch_only_balance;
    const CAmount total = confirmed + pending;
    m_confirmedValue->setText(BitcoinUnits::formatResWithPrivacy(confirmed, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_pendingValue->setText(BitcoinUnits::formatResWithPrivacy(pending, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_totalValue->setText(BitcoinUnits::formatResWithPrivacy(total, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
}

} // namespace RigorWallet
