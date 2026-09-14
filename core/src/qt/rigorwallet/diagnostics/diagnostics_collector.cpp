// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>

#include <qt/rigorwallet/explorer/explorerclient.h>
#include <qt/rigorwallet/walletidentity.h>

#include <chainparams.h>
#include <interfaces/node.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <univalue.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace RigorWallet {

namespace {

QString readProof034ObservationPath()
{
    QStringList candidates;
    candidates << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
        QStringLiteral("../../.runtime/proof034/proof034_observation.json"));
    candidates << QDir::home().absoluteFilePath(QStringLiteral(".runtime/proof034/proof034_observation.json"));

    const QByteArray rigor_root = qgetenv("RIGOR_ROOT");
    if (!rigor_root.isEmpty()) {
        candidates << QDir(QString::fromUtf8(rigor_root)).absoluteFilePath(
            QStringLiteral(".runtime/proof034/proof034_observation.json"));
    }

    for (const QString& path : candidates) {
        if (QFile::exists(path)) return path;
    }
    return QString();
}

QJsonObject loadProof034Observation()
{
    const QString path = readProof034ObservationPath();
    if (path.isEmpty()) return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

QString jsonString(const QJsonObject& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace

NodeStatusSnapshot DiagnosticsCollector::collectNode(ClientModel* clientModel)
{
    NodeStatusSnapshot snap;
    if (!clientModel) return snap;

    snap.block_height = clientModel->getNumBlocks();
    snap.headers = clientModel->getHeaderTipHeight();
    snap.peers = clientModel->getNumConnections();
    snap.initial_block_download = clientModel->node().isInitialBlockDownload();
    snap.network_active = clientModel->node().getNetworkActive();

    try {
        UniValue chain = clientModel->node().executeRpc("getblockchaininfo", UniValue(UniValue::VARR), "/");
        if (chain.isObject()) {
            if (chain.exists("headers")) snap.headers = chain["headers"].getInt<int>();
            if (chain.exists("verificationprogress")) {
                snap.sync_progress = chain["verificationprogress"].get_real();
            }
            if (chain.exists("bestblockhash")) {
                snap.best_block_hash = QString::fromStdString(chain["bestblockhash"].get_str());
            }
        }
    } catch (...) {
    }

    if (snap.peers <= 0 && !Params().IsMockableChain()) {
        snap.overall = StatusBadgeColor::Red;
    } else if (snap.initial_block_download || snap.block_height < snap.headers) {
        snap.overall = StatusBadgeColor::Yellow;
    } else {
        snap.overall = StatusBadgeColor::Green;
    }
    return snap;
}

WalletStatusSnapshot DiagnosticsCollector::collectWallet(WalletModel* walletModel)
{
    WalletStatusSnapshot snap;
    if (!walletModel) return snap;

    snap.loaded = true;
    const WalletModel::EncryptionStatus enc = walletModel->getEncryptionStatus();
    snap.encrypted = enc != WalletModel::Unencrypted && enc != WalletModel::NoKeys;
    snap.unlocked = enc == WalletModel::Unlocked || enc == WalletModel::Unencrypted;

    snap.backup_status = QCoreApplication::translate(
        "DiagnosticsCollector",
        "Not recorded — use File → Backup Wallet");

    if (walletModel->wallet().privateKeysDisabled()) {
        snap.unlocked = false;
    }

    if (!snap.unlocked && snap.encrypted) {
        snap.overall = StatusBadgeColor::Yellow;
    } else if (snap.loaded) {
        snap.overall = StatusBadgeColor::Green;
    } else {
        snap.overall = StatusBadgeColor::Gray;
    }
    return snap;
}

ExplorerStatusSnapshot DiagnosticsCollector::collectExplorer(ClientModel* clientModel)
{
    ExplorerStatusSnapshot snap;
    const ExplorerLiveStatus live = ExplorerClient::fetchLiveStatus(clientModel);
    snap.api_base_url = live.api_base_url;
    snap.connected = live.connected;
    snap.latest_indexed_height = live.indexed_height;
    snap.indexer_lag = live.indexer_lag;

    if (!live.configured) {
        snap.overall = StatusBadgeColor::Gray;
    } else if (!live.connected) {
        snap.overall = StatusBadgeColor::Red;
    } else if (live.indexer_lag > 10) {
        snap.overall = StatusBadgeColor::Yellow;
    } else {
        snap.overall = StatusBadgeColor::Green;
    }
    return snap;
}

ConvergenceDiagnosticsSnapshot DiagnosticsCollector::collectConvergence(ClientModel* clientModel)
{
    ConvergenceDiagnosticsSnapshot snap;
    if (clientModel) {
        try {
            UniValue r = clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
            if (r.isObject()) {
                const int tip = r["tip_height"].getInt<int>();
                const int last_res = r["last_res_height"].getInt<int>();
                if (tip >= 0 && last_res >= 0) snap.mint_lag = std::max(0, tip - last_res);
            }
        } catch (...) {
        }
    }

    if (snap.mint_lag > 100) {
        snap.overall = StatusBadgeColor::Yellow;
    } else {
        snap.overall = StatusBadgeColor::Green;
    }
    return snap;
}

VerificationStatusSnapshot DiagnosticsCollector::collectVerification(ClientModel* clientModel)
{
    VerificationStatusSnapshot snap;
    snap.equivalence_claimed = false;

    const QJsonObject obs = loadProof034Observation();
    if (!obs.isEmpty()) {
        snap.proof034_running = true;
        const QJsonObject prog = obs.value(QStringLiteral("proof_progress")).toObject();
        snap.proof_current_height = prog.value(QStringLiteral("current_height")).toInt(-1);
        snap.proof_target_height = prog.value(QStringLiteral("target_height")).toInt(2100001);
        snap.proof_progress_pct = prog.value(QStringLiteral("progress_percentage")).toDouble(0.0);
        const bool complete = prog.value(QStringLiteral("complete")).toBool(false);
        snap.archive_state = complete
            ? QCoreApplication::translate("DiagnosticsCollector", "Archive target reached")
            : QCoreApplication::translate("DiagnosticsCollector", "Archive in progress");
        snap.simulation_readiness = QCoreApplication::translate(
            "DiagnosticsCollector", "Observer snapshot available (simulation)");
    } else {
        snap.proof034_running = true;
        snap.archive_state = QCoreApplication::translate("DiagnosticsCollector", "Archive incomplete");
        snap.simulation_readiness = QCoreApplication::translate(
            "DiagnosticsCollector", "Simulation not complete — observer file unavailable");
        if (clientModel) {
            snap.proof_current_height = clientModel->getNumBlocks();
            if (snap.proof_target_height > 0) {
                snap.proof_progress_pct = 100.0 * snap.proof_current_height / snap.proof_target_height;
            }
        }
    }

    snap.overall = (snap.proof_progress_pct >= 100.0)
        ? StatusBadgeColor::Green
        : StatusBadgeColor::Yellow;
    return snap;
}

QString DiagnosticsCollector::nodeStatusJson(const NodeStatusSnapshot& snap)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("authority"), QStringLiteral("node"));
    obj.insert(QStringLiteral("block_height"), snap.block_height);
    obj.insert(QStringLiteral("headers"), snap.headers);
    obj.insert(QStringLiteral("peers"), snap.peers);
    obj.insert(QStringLiteral("sync_progress"), snap.sync_progress);
    obj.insert(QStringLiteral("best_block_hash"), snap.best_block_hash);
    obj.insert(QStringLiteral("initial_block_download"), snap.initial_block_download);
    obj.insert(QStringLiteral("network_active"), snap.network_active);
    obj.insert(QStringLiteral("overall"), StatusBadge::labelForColor(snap.overall));
    return jsonString(obj);
}

QString DiagnosticsCollector::walletStatusJson(const WalletStatusSnapshot& snap)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("authority"), QStringLiteral("wallet"));
    obj.insert(QStringLiteral("loaded"), snap.loaded);
    obj.insert(QStringLiteral("encrypted"), snap.encrypted);
    obj.insert(QStringLiteral("unlocked"), snap.unlocked);
    obj.insert(QStringLiteral("backup_status"), snap.backup_status);
    obj.insert(QStringLiteral("overall"), StatusBadge::labelForColor(snap.overall));
    return jsonString(obj);
}

QString DiagnosticsCollector::convergenceStatusJson(const ConvergenceDiagnosticsSnapshot& snap)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("authority"), QStringLiteral("node"));
    obj.insert(QStringLiteral("mint_lag"), snap.mint_lag);
    obj.insert(QStringLiteral("overall"), StatusBadge::labelForColor(snap.overall));
    return jsonString(obj);
}

QString DiagnosticsCollector::verificationStatusJson(const VerificationStatusSnapshot& snap)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("authority"), QStringLiteral("simulation"));
    obj.insert(QStringLiteral("proof034_running"), snap.proof034_running);
    obj.insert(QStringLiteral("archive_state"), snap.archive_state);
    obj.insert(QStringLiteral("simulation_readiness"), snap.simulation_readiness);
    obj.insert(QStringLiteral("proof_current_height"), snap.proof_current_height);
    obj.insert(QStringLiteral("proof_target_height"), snap.proof_target_height);
    obj.insert(QStringLiteral("proof_progress_pct"), snap.proof_progress_pct);
    obj.insert(QStringLiteral("equivalence_claimed"), snap.equivalence_claimed);
    obj.insert(QStringLiteral("overall"), StatusBadge::labelForColor(snap.overall));
    return jsonString(obj);
}

QString DiagnosticsCollector::explorerStatusJson(ClientModel* clientModel)
{
    const ExplorerLiveStatus live = ExplorerClient::fetchLiveStatus(clientModel);
    QJsonObject obj;
    obj.insert(QStringLiteral("authority"), QStringLiteral("indexed"));
    obj.insert(QStringLiteral("configured"), live.configured);
    obj.insert(QStringLiteral("connected"), live.connected);
    obj.insert(QStringLiteral("reachable"), live.reachable);
    obj.insert(QStringLiteral("indexed_height"), live.indexed_height);
    obj.insert(QStringLiteral("node_height"), live.node_height);
    obj.insert(QStringLiteral("indexer_lag"), live.indexer_lag);
    obj.insert(QStringLiteral("response_latency_ms"), live.response_latency_ms);
    obj.insert(QStringLiteral("api_base_url"), live.api_base_url);
    obj.insert(QStringLiteral("critical_root_mismatch"), live.critical_root_mismatch);
    obj.insert(QStringLiteral("equivalence_claimed"), false);
    return jsonString(obj);
}

} // namespace RigorWallet
