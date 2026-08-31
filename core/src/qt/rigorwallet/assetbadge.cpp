// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/assetbadge.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>

#include <wallet/tx_semantic.h>

#include <QCoreApplication>

namespace RigorWallet {

QString AssetBadge::labelForKind(AssetBadgeKind kind)
{
    switch (kind) {
    case AssetBadgeKind::ORU:
        return QCoreApplication::translate("AssetBadge", "ORU");
    case AssetBadgeKind::RES:
        return QCoreApplication::translate("AssetBadge", "RES");
    case AssetBadgeKind::CVG:
        return QCoreApplication::translate("AssetBadge", "Convergence");
    case AssetBadgeKind::MINT:
        return QCoreApplication::translate("AssetBadge", "Mint");
    case AssetBadgeKind::Unknown:
        return QCoreApplication::translate("AssetBadge", "Other");
    }
    return QCoreApplication::translate("AssetBadge", "Other");
}

AssetBadge::AssetBadge(QWidget* parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setKind(AssetBadgeKind::Unknown);
}

AssetBadgeKind AssetBadge::fromSemantic(wallet::TxSemanticKind semantic)
{
    using wallet::TxSemanticKind;
    switch (semantic) {
    case TxSemanticKind::OruCoinbase:
    case TxSemanticKind::OruTransfer:
        return AssetBadgeKind::ORU;
    case TxSemanticKind::ResTransfer:
        return AssetBadgeKind::RES;
    case TxSemanticKind::OruConvergence:
        return AssetBadgeKind::CVG;
    case TxSemanticKind::ResMint:
        return AssetBadgeKind::MINT;
    case TxSemanticKind::MixedInputDomain:
    case TxSemanticKind::Unknown:
        return AssetBadgeKind::Unknown;
    }
    return AssetBadgeKind::Unknown;
}

void AssetBadge::setKind(AssetBadgeKind kind)
{
    m_kind = kind;
    setText(labelForKind(kind));

    const ColorPalette p = RigorTheme::colors();
    const Spacing s = RigorTheme::spacing();
    QColor bg;
    QColor fg = p.textPrimary;

    switch (kind) {
    case AssetBadgeKind::ORU:
        bg = p.assetOruBg;
        fg = p.assetOru;
        break;
    case AssetBadgeKind::RES:
        bg = p.assetResBg;
        fg = p.assetRes;
        break;
    case AssetBadgeKind::CVG:
        bg = p.convergenceBg;
        fg = p.convergence;
        break;
    case AssetBadgeKind::MINT:
        bg = p.assetResBg;
        fg = p.assetRes;
        break;
    case AssetBadgeKind::Unknown:
        bg = p.bgElevated;
        fg = p.textMuted;
        break;
    }

    setStyleSheet(QStringLiteral(
        "QLabel {"
        " background-color: %1;"
        " color: %2;"
        " border: 1px solid %2;"
        " border-radius: %3px;"
        " padding: 2px 6px;"
        " font-size: %4px;"
        " font-weight: bold;"
        "}")
                      .arg(RigorTheme::colorHex(bg), RigorTheme::colorHex(fg))
                      .arg(s.borderRadiusChip)
                      .arg(RigorTheme::typography().statusEnumPx));
}

} // namespace RigorWallet
