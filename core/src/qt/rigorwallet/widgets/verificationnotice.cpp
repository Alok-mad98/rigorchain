// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/verificationnotice.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/walletidentity.h>

#include <QVBoxLayout>

namespace RigorWallet {

VerificationNotice::VerificationNotice(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(RigorTheme::spacing().unit / 2);

    m_experimentalBanner = new WarningBanner(this);
    m_layout->addWidget(m_experimentalBanner);

    buildNotices();
}

void VerificationNotice::buildNotices()
{
    const BrandInfo brand = WalletIdentity::brand();
    m_experimentalBanner->showBanner(
        WarningLevel::Info,
        tr("%1 is experimental software. Always verify balances and Convergence results against your own full node.")
            .arg(brand.productName));
}

void VerificationNotice::refresh()
{
    buildNotices();
}

} // namespace RigorWallet
