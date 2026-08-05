// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_ASSETBADGE_H
#define BITCOIN_QT_RIGORWALLET_ASSETBADGE_H

#include <QLabel>
#include <QString>

namespace wallet {
enum class TxSemanticKind : uint8_t;
}

namespace RigorWallet {

/** Visual badge kinds for transaction and panel classification (BUILD-14). */
enum class AssetBadgeKind {
    ORU,
    RES,
    CVG,
    MINT,
    Unknown,
};

class AssetBadge : public QLabel
{
    Q_OBJECT

public:
    explicit AssetBadge(QWidget* parent = nullptr);

    AssetBadgeKind kind() const { return m_kind; }
    void setKind(AssetBadgeKind kind);

    static AssetBadgeKind fromSemantic(wallet::TxSemanticKind semantic);
    static QString labelForKind(AssetBadgeKind kind);

private:
    AssetBadgeKind m_kind{AssetBadgeKind::Unknown};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_ASSETBADGE_H
