// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/orubalancepanel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>

#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

ORUBalancePanel::ORUBalancePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorOruBalancePanel"));
    applyTheme();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding,
                               RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding);
    layout->setSpacing(RigorTheme::spacing().unit);

    m_titleLabel = new QLabel(AssetContext::panelTitle(WalletAssetContext::ORU, DataAuthority::Node), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPixelSize(RigorTheme::typography().headingPx);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(RigorTheme::spacing().sectionGap);
    grid->setVerticalSpacing(RigorTheme::spacing().unit);

    m_confirmedLabel = new QLabel(tr("Confirmed"), this);
    m_unconfirmedLabel = new QLabel(tr("Unconfirmed"), this);
    m_pendingLabel = new QLabel(tr("Pending"), this);
    m_totalLabel = new QLabel(tr("Total"), this);

    m_confirmedValue = new QLabel(QStringLiteral("—"), this);
    m_unconfirmedValue = new QLabel(QStringLiteral("—"), this);
    m_pendingValue = new QLabel(QStringLiteral("—"), this);
    m_totalValue = new QLabel(QStringLiteral("—"), this);

    QFont valueFont = GUIUtil::fixedPitchFont(false);
    valueFont.setBold(true);
    valueFont.setPixelSize(RigorTheme::typography().balancePx);
    for (QLabel* v : {m_confirmedValue, m_unconfirmedValue, m_pendingValue, m_totalValue}) {
        v->setFont(valueFont);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    grid->addWidget(m_confirmedLabel, 0, 0);
    grid->addWidget(m_confirmedValue, 0, 1);
    grid->addWidget(m_unconfirmedLabel, 1, 0);
    grid->addWidget(m_unconfirmedValue, 1, 1);
    grid->addWidget(m_pendingLabel, 2, 0);
    grid->addWidget(m_pendingValue, 2, 1);
    grid->addWidget(m_totalLabel, 3, 0);
    grid->addWidget(m_totalValue, 3, 1);
    layout->addLayout(grid);
}

void ORUBalancePanel::applyTheme()
{
    const ColorPalette p = RigorTheme::colors();
    const Spacing s = RigorTheme::spacing();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorOruBalancePanel {"
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
                      .arg(RigorTheme::colorHex(p.assetOru))
                      .arg(RigorTheme::colorHex(p.borderSubtle))
                      .arg(s.borderRadiusPanel)
                      .arg(RigorTheme::colorHex(p.textPrimary)));
}

void ORUBalancePanel::setBalances(const interfaces::WalletBalances& balances, BitcoinUnit unit, bool privacy)
{
    const CAmount confirmed = balances.oru_balance + balances.watch_only_balance;
    const CAmount unconfirmed = balances.oru_unconfirmed_balance + balances.unconfirmed_watch_only_balance;
    const CAmount pending = balances.immature_balance + balances.immature_watch_only_balance;
    const CAmount total = confirmed + unconfirmed + pending;

    m_confirmedValue->setText(BitcoinUnits::formatWithPrivacy(unit, confirmed, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_unconfirmedValue->setText(BitcoinUnits::formatWithPrivacy(unit, unconfirmed, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_pendingValue->setText(BitcoinUnits::formatWithPrivacy(unit, pending, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
    m_totalValue->setText(BitcoinUnits::formatWithPrivacy(unit, total, BitcoinUnits::SeparatorStyle::ALWAYS, privacy));
}

} // namespace RigorWallet
