// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORERCLIENT_H
#define BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORERCLIENT_H

#include <QJsonObject>
#include <QString>

class ClientModel;

namespace RigorWallet {

/** Indexed transaction verification state (BUILD-43). */
enum class ExplorerTxVerification {
    Unknown,
    VerifiedIndexed,
    NotIndexed,
    ExplorerUnavailable,
};

/** Result of a read-only explorer HTTP GET. */
struct ExplorerHttpResult {
    bool ok{false};
    int http_status{0};
    qint64 latency_ms{-1};
    QJsonObject body;
    QString error;
};

/** Parsed explorer connection snapshot (BUILD-39). */
struct ExplorerLiveStatus {
    bool configured{false};
    bool connected{false};
    bool reachable{false};
    int indexed_height{-1};
    int node_height{-1};
    int indexer_lag{-1};
    qint64 response_latency_ms{-1};
    QString api_base_url;
    bool critical_root_mismatch{false};
    bool equivalence_claimed{false};
};

/** Centralized read-only explorer access (BUILD-38). */
class ExplorerClient
{
public:
    ExplorerClient() = delete;

    static QString apiBaseUrl();
    static bool isConfigured();

    static ExplorerHttpResult httpGet(const QString& path);
    static ExplorerLiveStatus fetchLiveStatus(ClientModel* clientModel);

    static ExplorerTxVerification verifyTransactionIndexed(const QString& txid);
    static bool lookupTransaction(const QString& txid, QJsonObject* out);
    static bool lookupConvergence(const QString& txid, QJsonObject* out);
    static bool lookupMint(const QString& txid, QJsonObject* out);

    static QString browseTransactionUrl(const QString& txid);
    static QString browseConvergenceUrl(const QString& txid);
    static QString browseMintUrl(const QString& txid);
    static QString browseStatusUrl();

    static QString verificationLabel(ExplorerTxVerification state);
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORERCLIENT_H
