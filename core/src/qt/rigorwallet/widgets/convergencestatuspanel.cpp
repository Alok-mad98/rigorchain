// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergencestatuspanel.h>

#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/walletmodel.h>

#include <interfaces/wallet.h>
#include <wallet/tx_semantic.h>

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString TruncateMiddleHex(const QString& hex, int head = 12, int tail = 12)
{
    if (hex.size() <= head + tail + 1) return hex;
    return hex.left(head) + QStringLiteral("\u2026") + hex.right(tail);
}
} // namespace

ConvergenceStatusPanel::ConvergenceStatusPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceStatusPanel"));
    applyTheme();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding,
                               RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding);
    layout->setSpacing(RigorTheme::spacing().unit / 2);

    m_titleLabel = new QLabel(AssetContext::panelTitle(WalletAssetContext::Convergence, DataAuthority::Node), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPixelSize(RigorTheme::typography().headingPx);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel(tr("No convergence transactions in wallet."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setFont(GUIUtil::fixedPitchFont(false));
    m_detailLabel->setVisible(false);
    layout->addWidget(m_detailLabel);
}

void ConvergenceStatusPanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void ConvergenceStatusPanel::applyTheme()
{
    const ColorPalette p = RigorTheme::colors();
    const Spacing s = RigorTheme::spacing();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorConvergenceStatusPanel {"
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
                      .arg(RigorTheme::colorHex(p.convergence))
                      .arg(RigorTheme::colorHex(p.borderSubtle))
                      .arg(s.borderRadiusPanel)
                      .arg(RigorTheme::colorHex(p.textPrimary)));
}

void ConvergenceStatusPanel::refresh()
{
    if (!m_walletModel) {
        m_statusLabel->setText(tr("No convergence transactions in wallet."));
        m_detailLabel->setVisible(false);
        return;
    }

    int count = 0;
    QString latest_hash;
    qint64 latest_time = 0;
    const std::set<interfaces::WalletTx> txs = m_walletModel->wallet().getWalletTxs();
    for (const interfaces::WalletTx& wtx : txs) {
        if (wtx.semantic != wallet::TxSemanticKind::OruConvergence) continue;
        ++count;
        if (wtx.time >= latest_time) {
            latest_time = wtx.time;
            latest_hash = QString::fromStdString(wtx.tx->GetHash().GetHex());
        }
    }

    if (count == 0) {
        m_statusLabel->setText(tr("No convergence transactions in wallet."));
        m_detailLabel->setVisible(false);
        return;
    }

    m_statusLabel->setText(tr("%n convergence transaction(s) in wallet history.", "", count));
    m_detailLabel->setText(tr("Latest: %1").arg(TruncateMiddleHex(latest_hash)));
    m_detailLabel->setVisible(true);
}

} // namespace RigorWallet
