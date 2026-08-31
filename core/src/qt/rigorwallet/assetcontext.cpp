// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>

#include <wallet/tx_semantic.h>

namespace RigorWallet {

WalletAssetContext AssetContext::fromAssetType(AssetType asset)
{
    return asset == ASSET_RES ? WalletAssetContext::RES : WalletAssetContext::ORU;
}

AssetType AssetContext::toAssetType(WalletAssetContext ctx)
{
    switch (ctx) {
    case WalletAssetContext::RES:
        return ASSET_RES;
    case WalletAssetContext::ORU:
    case WalletAssetContext::Convergence:
        return ASSET_ORU;
    }
    return ASSET_ORU;
}

QString AssetContext::displayName(WalletAssetContext ctx)
{
    switch (ctx) {
    case WalletAssetContext::ORU:
        return QStringLiteral("ORU");
    case WalletAssetContext::RES:
        return QStringLiteral("RES");
    case WalletAssetContext::Convergence:
        return QStringLiteral("Convergence");
    }
    return QStringLiteral("ORU");
}

QString AssetContext::shortLabel(WalletAssetContext ctx)
{
    return displayName(ctx);
}

QColor AssetContext::accentColor(WalletAssetContext ctx)
{
    const ColorPalette p = RigorTheme::colors();
    switch (ctx) {
    case WalletAssetContext::RES:
        return p.assetRes;
    case WalletAssetContext::Convergence:
        return p.convergence;
    case WalletAssetContext::ORU:
        return p.assetOru;
    }
    return p.assetOru;
}

QColor AssetContext::backgroundTint(WalletAssetContext ctx)
{
    const ColorPalette p = RigorTheme::colors();
    switch (ctx) {
    case WalletAssetContext::RES:
        return p.assetResBg;
    case WalletAssetContext::Convergence:
        return p.convergenceBg;
    case WalletAssetContext::ORU:
        return p.assetOruBg;
    }
    return p.assetOruBg;
}

QString AssetContext::panelBorderStyle(WalletAssetContext ctx)
{
    const Spacing s = RigorTheme::spacing();
    const QString color = RigorTheme::colorHex(accentColor(ctx));
    return QStringLiteral("border-left: %1px solid %2;").arg(s.assetBorderWidth).arg(color);
}

bool AssetContext::isTransferAsset(WalletAssetContext ctx)
{
    return ctx == WalletAssetContext::ORU || ctx == WalletAssetContext::RES;
}

bool AssetContext::isConvergence(WalletAssetContext ctx)
{
    return ctx == WalletAssetContext::Convergence;
}

WalletAssetContext AssetContext::fromSemantic(wallet::TxSemanticKind semantic)
{
    using wallet::TxSemanticKind;
    switch (semantic) {
    case TxSemanticKind::OruConvergence:
        return WalletAssetContext::Convergence;
    case TxSemanticKind::ResTransfer:
    case TxSemanticKind::ResMint:
        return WalletAssetContext::RES;
    case TxSemanticKind::OruCoinbase:
    case TxSemanticKind::OruTransfer:
        return WalletAssetContext::ORU;
    case TxSemanticKind::MixedInputDomain:
    case TxSemanticKind::Unknown:
        break;
    }
    return WalletAssetContext::ORU;
}

AssetBadgeKind AssetContext::badgeKindFromSemantic(wallet::TxSemanticKind semantic)
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

QString AssetContext::authorityLabel(DataAuthority authority)
{
    switch (authority) {
    case DataAuthority::Node:
        // Product overview panels: plain "ORU" / "RES" / "Convergence" (no "(node)").
        return {};
    case DataAuthority::Indexed:
        return QStringLiteral("(indexed)");
    case DataAuthority::Wallet:
        return QStringLiteral("(wallet)");
    case DataAuthority::Simulation:
        return QStringLiteral("(simulation)");
    }
    return {};
}

QString AssetContext::panelTitle(WalletAssetContext ctx, DataAuthority authority)
{
    const QString auth = authorityLabel(authority);
    if (auth.isEmpty()) return displayName(ctx);
    return displayName(ctx) + QStringLiteral(" ") + auth;
}

} // namespace RigorWallet
