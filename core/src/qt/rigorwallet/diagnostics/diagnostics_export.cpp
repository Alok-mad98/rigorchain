// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/diagnostics/diagnostics_export.h>

#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace RigorWallet {

namespace {

bool writeTextFile(const QString& path, const QString& contents, QString* error_out)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error_out) *error_out = file.errorString();
        return false;
    }
    if (file.write(contents.toUtf8()) < 0) {
        if (error_out) *error_out = file.errorString();
        return false;
    }
    return true;
}

QString walletDiagnosticsJson(ClientModel* clientModel, WalletModel* walletModel)
{
    const NodeStatusSnapshot node = DiagnosticsCollector::collectNode(clientModel);
    const WalletStatusSnapshot wallet = DiagnosticsCollector::collectWallet(walletModel);
    const ConvergenceDiagnosticsSnapshot convergence =
        DiagnosticsCollector::collectConvergence(clientModel, walletModel);
    const VerificationStatusSnapshot verification =
        DiagnosticsCollector::collectVerification(clientModel);

    QJsonObject root;
    root.insert(QStringLiteral("equivalence_claimed"), false);
    root.insert(QStringLiteral("node"),
                QJsonDocument::fromJson(DiagnosticsCollector::nodeStatusJson(node).toUtf8()).object());
    root.insert(QStringLiteral("wallet"),
                QJsonDocument::fromJson(DiagnosticsCollector::walletStatusJson(wallet).toUtf8()).object());

    QJsonObject explorer_obj = QJsonDocument::fromJson(
        DiagnosticsCollector::explorerStatusJson(clientModel).toUtf8()).object();
    root.insert(QStringLiteral("explorer"), explorer_obj);

    root.insert(QStringLiteral("convergence"),
                QJsonDocument::fromJson(DiagnosticsCollector::convergenceStatusJson(convergence).toUtf8()).object());
    root.insert(QStringLiteral("verification"),
                QJsonDocument::fromJson(DiagnosticsCollector::verificationStatusJson(verification).toUtf8()).object());
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace

bool DiagnosticsExport::exportBundle(const QString& directory,
                                     ClientModel* clientModel,
                                     WalletModel* walletModel,
                                     QString* error_out)
{
    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error_out) *error_out = QObject::tr("Could not create export directory.");
        return false;
    }

    const NodeStatusSnapshot node = DiagnosticsCollector::collectNode(clientModel);
    const WalletStatusSnapshot wallet = DiagnosticsCollector::collectWallet(walletModel);
    const ConvergenceDiagnosticsSnapshot convergence =
        DiagnosticsCollector::collectConvergence(clientModel, walletModel);
    const VerificationStatusSnapshot verification =
        DiagnosticsCollector::collectVerification(clientModel);

    if (!writeTextFile(dir.filePath(QStringLiteral("wallet_diagnostics.json")),
            walletDiagnosticsJson(clientModel, walletModel), error_out)) {
        return false;
    }
    if (!writeTextFile(dir.filePath(QStringLiteral("node_status.json")),
            DiagnosticsCollector::nodeStatusJson(node), error_out)) {
        return false;
    }
    if (!writeTextFile(dir.filePath(QStringLiteral("explorer_status.json")),
            DiagnosticsCollector::explorerStatusJson(clientModel), error_out)) {
        return false;
    }
    if (!writeTextFile(dir.filePath(QStringLiteral("convergence_status.json")),
            DiagnosticsCollector::convergenceStatusJson(convergence), error_out)) {
        return false;
    }
    if (!writeTextFile(dir.filePath(QStringLiteral("verification_status.json")),
            DiagnosticsCollector::verificationStatusJson(verification), error_out)) {
        return false;
    }
    return true;
}

} // namespace RigorWallet
