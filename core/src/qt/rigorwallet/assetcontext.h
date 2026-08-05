// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_ASSETCONTEXT_H
#define BITCOIN_QT_RIGORWALLET_ASSETCONTEXT_H

#include <qt/rigorwallet/assetbadge.h>

#include <primitives/transaction.h>

#include <wallet/tx_semantic.h>

#include <QColor>
#include <QString>

namespace RigorWallet {

/** Data authority for displayed values (BUILD-17, BUILD-36). */
enum class DataAuthority {
    Node,
    Indexed,
    Wallet,
    Simulation,
};

/** Wallet UI asset context — maps protocol AssetType + Convergence surface to presentation. */
enum class WalletAssetContext {
    ORU,
    RES,
    Convergence,
};

/** Presentation helpers for dual-native + convergence UI (no protocol semantics). */
class AssetContext
{
public:
    AssetContext() = delete;

    static WalletAssetContext fromAssetType(AssetType asset);
    static AssetType toAssetType(WalletAssetContext ctx);

    static QString displayName(WalletAssetContext ctx);
    static QString shortLabel(WalletAssetContext ctx);
    static QColor accentColor(WalletAssetContext ctx);
    static QColor backgroundTint(WalletAssetContext ctx);
    static QString panelBorderStyle(WalletAssetContext ctx);

    static bool isTransferAsset(WalletAssetContext ctx);
    static bool isConvergence(WalletAssetContext ctx);

    static WalletAssetContext fromSemantic(wallet::TxSemanticKind semantic);
    static AssetBadgeKind badgeKindFromSemantic(wallet::TxSemanticKind semantic);
    static QString authorityLabel(DataAuthority authority);
    static QString panelTitle(WalletAssetContext ctx, DataAuthority authority);
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_ASSETCONTEXT_H
