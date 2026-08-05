// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/explorer/explorerclient.h>

#include <qt/clientmodel.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace RigorWallet {

namespace {

QString normalizedBaseUrl(const QString& raw)
{
    QString base = raw.trimmed();
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base;
}

ExplorerHttpResult performGet(const QUrl& url)
{
    ExplorerHttpResult result;
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RIGOR-Wallet-ExplorerClient"));

    QElapsedTimer timer;
    timer.start();

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    result.latency_ms = timer.elapsed();
    result.http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        result.error = reply->errorString();
        reply->deleteLater();
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    if (!doc.isObject()) {
        result.error = QCoreApplication::translate("ExplorerClient", "Invalid JSON response");
        return result;
    }

    result.body = doc.object();
    result.ok = result.http_status >= 200 && result.http_status < 300;
    return result;
}

int jsonInt(const QJsonObject& obj, const char* key, int fallback = -1)
{
    const QJsonValue v = obj.value(QString::fromUtf8(key));
    if (v.isDouble()) return v.toInt();
    if (v.isString()) return v.toString().toInt();
    return fallback;
}

} // namespace

QString ExplorerClient::apiBaseUrl()
{
    QSettings settings;
    return normalizedBaseUrl(settings.value(QStringLiteral("RigorExplorer/ApiBaseUrl")).toString());
}

bool ExplorerClient::isConfigured()
{
    return !apiBaseUrl().isEmpty();
}

ExplorerHttpResult ExplorerClient::httpGet(const QString& path)
{
    const QString base = apiBaseUrl();
    if (base.isEmpty()) {
        ExplorerHttpResult r;
        r.error = QCoreApplication::translate("ExplorerClient", "Explorer URL not configured");
        return r;
    }
    const QString p = path.startsWith(QLatin1Char('/')) ? path : QStringLiteral("/") + path;
    return performGet(QUrl(base + p));
}

ExplorerLiveStatus ExplorerClient::fetchLiveStatus(ClientModel* clientModel)
{
    ExplorerLiveStatus status;
    status.api_base_url = apiBaseUrl();
    status.configured = isConfigured();
    if (clientModel) status.node_height = clientModel->getNumBlocks();

    if (!status.configured) return status;

    const ExplorerHttpResult health = httpGet(QStringLiteral("/healthz"));
    status.response_latency_ms = health.latency_ms;
    status.reachable = health.ok;
    status.connected = health.ok;

    if (health.ok) {
        const QJsonObject chain = health.body.value(QStringLiteral("chain")).toObject();
        status.indexed_height = jsonInt(chain, "last_height");
        if (status.indexed_height < 0) {
            status.indexed_height = jsonInt(chain, "height");
        }
    }

    const ExplorerHttpResult api_status = httpGet(QStringLiteral("/api/status"));
    if (api_status.ok) {
        status.connected = true;
        if (status.response_latency_ms < 0) status.response_latency_ms = api_status.latency_ms;

        const QJsonObject block_health = api_status.body.value(QStringLiteral("block_health")).toObject();
        const int idx = jsonInt(block_health, "current_height_indexer");
        const int node = jsonInt(block_health, "current_height_node");
        if (idx >= 0) status.indexed_height = idx;
        if (node >= 0 && status.node_height < 0) status.node_height = node;

        const QJsonObject integrity = api_status.body.value(QStringLiteral("system_integrity")).toObject();
        status.critical_root_mismatch = integrity.value(QStringLiteral("critical_root_mismatch")).toBool(false);

        const QJsonObject chain_id = api_status.body.value(QStringLiteral("chain_identity")).toObject();
        status.equivalence_claimed = chain_id.value(QStringLiteral("equivalence_claimed")).toBool(false);
    } else if (!status.reachable) {
        status.connected = false;
    }

    if (status.node_height >= 0 && status.indexed_height >= 0) {
        status.indexer_lag = qMax(0, status.node_height - status.indexed_height);
    }
    return status;
}

ExplorerTxVerification ExplorerClient::verifyTransactionIndexed(const QString& txid)
{
    if (!isConfigured()) return ExplorerTxVerification::ExplorerUnavailable;
    if (txid.isEmpty()) return ExplorerTxVerification::Unknown;

    const ExplorerHttpResult r = httpGet(QStringLiteral("/api/tx/") + txid);
    if (r.http_status == 404) return ExplorerTxVerification::NotIndexed;
    if (r.ok) return ExplorerTxVerification::VerifiedIndexed;
    return ExplorerTxVerification::ExplorerUnavailable;
}

bool ExplorerClient::lookupTransaction(const QString& txid, QJsonObject* out)
{
    if (!out || txid.isEmpty()) return false;
    const ExplorerHttpResult r = httpGet(QStringLiteral("/api/tx/") + txid);
    if (!r.ok) return false;
    *out = r.body;
    return true;
}

bool ExplorerClient::lookupConvergence(const QString& txid, QJsonObject* out)
{
    if (!out || txid.isEmpty()) return false;
    ExplorerHttpResult r = httpGet(QStringLiteral("/api/lineage/") + txid);
    if (r.ok) {
        *out = r.body;
        return true;
    }
    r = httpGet(QStringLiteral("/api/tx/") + txid);
    if (!r.ok) return false;
    *out = r.body;
    return true;
}

bool ExplorerClient::lookupMint(const QString& txid, QJsonObject* out)
{
    if (!out || txid.isEmpty()) return false;
    if (lookupTransaction(txid, out)) return true;

    const ExplorerHttpResult r = httpGet(QStringLiteral("/api/res/mints?limit=200"));
    if (!r.ok) return false;
    const QJsonArray mints = r.body.value(QStringLiteral("mints")).toArray();
    for (const QJsonValue& v : mints) {
        const QJsonObject row = v.toObject();
        const QString row_tx = row.value(QStringLiteral("txid")).toString();
        const QString row_mint = row.value(QStringLiteral("mint_txid")).toString();
        if (row_tx.compare(txid, Qt::CaseInsensitive) == 0 ||
            row_mint.compare(txid, Qt::CaseInsensitive) == 0) {
            *out = row;
            return true;
        }
    }
    return false;
}

QString ExplorerClient::browseTransactionUrl(const QString& txid)
{
    const QString base = apiBaseUrl();
    if (base.isEmpty() || txid.isEmpty()) return QString();
    return base + QStringLiteral("/tx/") + txid;
}

QString ExplorerClient::browseConvergenceUrl(const QString& txid)
{
    return browseTransactionUrl(txid);
}

QString ExplorerClient::browseMintUrl(const QString& txid)
{
    return browseTransactionUrl(txid);
}

QString ExplorerClient::browseStatusUrl()
{
    const QString base = apiBaseUrl();
    if (base.isEmpty()) return QString();
    return base + QStringLiteral("/status");
}

QString ExplorerClient::verificationLabel(ExplorerTxVerification state)
{
    switch (state) {
    case ExplorerTxVerification::VerifiedIndexed:
        return QCoreApplication::translate("ExplorerClient", "Verified by explorer");
    case ExplorerTxVerification::NotIndexed:
        return QCoreApplication::translate("ExplorerClient", "Not indexed");
    case ExplorerTxVerification::ExplorerUnavailable:
        return QCoreApplication::translate("ExplorerClient", "Indexed data unavailable");
    case ExplorerTxVerification::Unknown:
        break;
    }
    return QCoreApplication::translate("ExplorerClient", "Unknown");
}

} // namespace RigorWallet
