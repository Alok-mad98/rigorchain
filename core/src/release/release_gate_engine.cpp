// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <release/release_gate_engine.h>

#include <chainparams.h>
#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/ui_readiness.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ReleaseGate {

namespace {

constexpr int kMaxLagBlocks = 10;

QString utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QJsonObject readJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

int countJsonlLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return 0;
    int lines = 0;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (!line.isEmpty()) ++lines;
    }
    return lines;
}

void addBlocker(Evaluation& eval, Level gate, const QString& code, const QString& message)
{
    Blocker b;
    b.gate_id = Engine::levelShortId(gate);
    b.code = code;
    b.message = message;
    eval.blockers.push_back(b);
}

bool checkG1(const ArtifactSignals& a, Evaluation& eval)
{
    bool ok = true;
    if (!a.ui_release_candidate_ready) {
        addBlocker(eval, Level::G1_UiComplete, QStringLiteral("G1-UI-01"),
            QCoreApplication::translate("ReleaseGate", "UI release candidate flag is not set."));
        ok = false;
    }
    if (!a.ui_finalization_report_present) {
        addBlocker(eval, Level::G1_UiComplete, QStringLiteral("G1-UI-02"),
            QCoreApplication::translate("ReleaseGate", "UI finalization report is missing."));
        ok = false;
    }
    return ok;
}

bool checkG2(const ArtifactSignals& a, Evaluation& eval)
{
    bool ok = true;
    if (!a.wallet_sim_pass) {
        addBlocker(eval, Level::G2_SimulationValidated, QStringLiteral("G2-SIM-01"),
            QCoreApplication::translate("ReleaseGate", "Wallet simulation stress test did not pass."));
        ok = false;
    }
    if (!a.gui_replay_consistent) {
        addBlocker(eval, Level::G2_SimulationValidated, QStringLiteral("G2-SIM-02"),
            QCoreApplication::translate("ReleaseGate", "GUI replay consistency check failed."));
        ok = false;
    }
    return ok;
}

bool checkG3(const ArtifactSignals& a, Evaluation& eval)
{
    if (!a.wallet_runtime_manifest_pass) {
        addBlocker(eval, Level::G3_RuntimeValidated, QStringLiteral("G3-RT-01"),
            QCoreApplication::translate("ReleaseGate", "Wallet runtime validation manifest did not pass."));
        return false;
    }
    return true;
}

bool checkG4(const ArtifactSignals& a,
             const RigorWallet::NodeStatusSnapshot& node,
             const RigorWallet::WalletStatusSnapshot& wallet,
             Evaluation& eval)
{
    bool ok = true;
    if (!a.state_authority_docs_present) {
        addBlocker(eval, Level::G4_ProductReady, QStringLiteral("G4-STATE-01"),
            QCoreApplication::translate("ReleaseGate", "State authority documentation set is incomplete."));
        ok = false;
    }
    if (!a.equivalence_claimed_false) {
        addBlocker(eval, Level::G4_ProductReady, QStringLiteral("G4-EQUIV-01"),
            QCoreApplication::translate("ReleaseGate", "equivalence_claimed must remain false."));
        ok = false;
    }
    if (!a.divergence_events_empty) {
        addBlocker(eval, Level::G4_ProductReady, QStringLiteral("G4-DIV-01"),
            QCoreApplication::translate("ReleaseGate", "Simulation divergence events detected."));
        ok = false;
    }
    if (!wallet.loaded) {
        addBlocker(eval, Level::G4_ProductReady, QStringLiteral("G4-WALLET-01"),
            QCoreApplication::translate("ReleaseGate", "Wallet is not loaded."));
        ok = false;
    }
    if (node.overall == RigorWallet::StatusBadgeColor::Red) {
        addBlocker(eval, Level::G4_ProductReady, QStringLiteral("G4-NODE-01"),
            QCoreApplication::translate("ReleaseGate", "Node diagnostics report a critical issue."));
        ok = false;
    }
    return ok;
}

bool checkG5(const ArtifactSignals&,
             const RigorWallet::ExplorerStatusSnapshot& explorer,
             const RigorWallet::ConvergenceDiagnosticsSnapshot& convergence,
             Evaluation& eval)
{
    bool ok = true;
    if (explorer.api_base_url.isEmpty()) {
        addBlocker(eval, Level::G5_TestnetCandidate, QStringLiteral("G5-EXP-01"),
            QCoreApplication::translate("ReleaseGate", "Explorer is not configured."));
        ok = false;
    } else if (!explorer.connected) {
        addBlocker(eval, Level::G5_TestnetCandidate, QStringLiteral("G5-EXP-02"),
            QCoreApplication::translate("ReleaseGate", "Explorer is not connected."));
        ok = false;
    } else if (explorer.indexer_lag > kMaxLagBlocks) {
        addBlocker(eval, Level::G5_TestnetCandidate, QStringLiteral("G5-EXP-03"),
            QCoreApplication::translate("ReleaseGate", "Explorer indexer lag exceeds allowed threshold."));
        ok = false;
    }
    if (convergence.overall == RigorWallet::StatusBadgeColor::Red) {
        addBlocker(eval, Level::G5_TestnetCandidate, QStringLiteral("G5-CVG-01"),
            QCoreApplication::translate("ReleaseGate", "Convergence validation failed."));
        ok = false;
    }
    return ok;
}

bool checkG6(const ArtifactSignals& a,
             const RigorWallet::VerificationStatusSnapshot& verification,
             Evaluation& eval)
{
    bool ok = true;
    if (!a.unified_sim_pass) {
        addBlocker(eval, Level::G6_MainnetCandidate, QStringLiteral("G6-UNI-01"),
            QCoreApplication::translate("ReleaseGate", "SIM-UNIFIED full system simulation did not pass."));
        ok = false;
    }
    if (verification.proof_progress_pct < 100.0) {
        addBlocker(eval, Level::G6_MainnetCandidate, QStringLiteral("G6-PROOF-01"),
            QCoreApplication::translate("ReleaseGate", "PROOF-034 archive verification is incomplete."));
        ok = false;
    }
    return ok;
}

QString deriveNextAction(const Evaluation& eval)
{
    if (eval.blockers.isEmpty()) {
        if (eval.testnet_release_ready) {
            return QCoreApplication::translate("ReleaseGate", "Testnet candidate criteria satisfied.");
        }
        return QCoreApplication::translate("ReleaseGate", "Advance to next gate requirements.");
    }
    return eval.blockers.first().message;
}

} // namespace

QString Engine::levelLabel(Level level)
{
    switch (level) {
    case Level::G0_Development:
        return QCoreApplication::translate("ReleaseGate", "G0 — Development");
    case Level::G1_UiComplete:
        return QCoreApplication::translate("ReleaseGate", "G1 — UI Complete");
    case Level::G2_SimulationValidated:
        return QCoreApplication::translate("ReleaseGate", "G2 — Simulation Validated");
    case Level::G3_RuntimeValidated:
        return QCoreApplication::translate("ReleaseGate", "G3 — Runtime Validated");
    case Level::G4_ProductReady:
        return QCoreApplication::translate("ReleaseGate", "G4 — Product Ready");
    case Level::G5_TestnetCandidate:
        return QCoreApplication::translate("ReleaseGate", "G5 — Testnet Candidate");
    case Level::G6_MainnetCandidate:
        return QCoreApplication::translate("ReleaseGate", "G6 — Mainnet Candidate");
    }
    return QStringLiteral("G0");
}

QString Engine::levelShortId(Level level)
{
    switch (level) {
    case Level::G0_Development: return QStringLiteral("G0");
    case Level::G1_UiComplete: return QStringLiteral("G1");
    case Level::G2_SimulationValidated: return QStringLiteral("G2");
    case Level::G3_RuntimeValidated: return QStringLiteral("G3");
    case Level::G4_ProductReady: return QStringLiteral("G4");
    case Level::G5_TestnetCandidate: return QStringLiteral("G5");
    case Level::G6_MainnetCandidate: return QStringLiteral("G6");
    }
    return QStringLiteral("G0");
}

QStringList Engine::searchRoots()
{
    QStringList roots;
    const QByteArray rigor_root = qgetenv("RIGOR_ROOT");
    if (!rigor_root.isEmpty()) {
        roots << QDir(QString::fromUtf8(rigor_root)).absolutePath();
    }
    roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../.."));
    roots << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../.."));
    roots << QDir::home().absolutePath();
    return roots;
}

QString Engine::resolveArtifactPath(const QString& relative_path)
{
    for (const QString& root : searchRoots()) {
        const QString candidate = QDir(root).absoluteFilePath(relative_path);
        if (QFile::exists(candidate)) return candidate;
    }
    return QString();
}

ArtifactSignals Engine::loadArtifacts()
{
    ArtifactSignals signals;
    signals.ui_release_candidate_ready = RigorWallet::UI_RELEASE_CANDIDATE_READY;
    signals.equivalence_claimed_false = true;

    signals.ui_finalization_report_present =
        !resolveArtifactPath(QStringLiteral("docs/wallet/RIGOR_WALLET_UI_FINALIZATION_REPORT.md")).isEmpty();

    const QStringList state_docs = {
        QStringLiteral("docs/state/RIGOR_STATE_AUTHORITY_MODEL.md"),
        QStringLiteral("docs/state/STATE_PRECEDENCE_RULES.md"),
        QStringLiteral("docs/state/STATE_CONFLICT_RESOLUTION_MATRIX.md"),
        QStringLiteral("docs/state/STATE_LABELING_STANDARD.md"),
        QStringLiteral("docs/state/CORE_WALLET_BOUNDARY_MAP.md"),
        QStringLiteral("docs/state/DETERMINISM_GUARANTEE_SPEC.md"),
    };
    signals.state_authority_docs_present = true;
    for (const QString& doc : state_docs) {
        if (resolveArtifactPath(doc).isEmpty()) {
            signals.state_authority_docs_present = false;
            break;
        }
    }

    const QJsonObject runtime_manifest = readJsonObject(
        resolveArtifactPath(QStringLiteral("docs/wallet/evidence/runtime/manifest.json")));
    signals.wallet_runtime_manifest_pass = runtime_manifest.value(QStringLiteral("overall_pass")).toBool(false);
    if (runtime_manifest.value(QStringLiteral("equivalence_claimed")).toBool(true)) {
        signals.equivalence_claimed_false = false;
    }

    const QJsonObject wallet_sim = readJsonObject(
        resolveArtifactPath(QStringLiteral(".runtime/wallet-sim/wallet_sim_report.json")));
    signals.wallet_sim_pass = wallet_sim.value(QStringLiteral("all_scenarios_passed")).toBool(false);
    signals.gui_replay_consistent = wallet_sim.value(QStringLiteral("gui_replay_consistent")).toBool(false);
    if (wallet_sim.value(QStringLiteral("equivalence_claimed")).toBool(true)) {
        signals.equivalence_claimed_false = false;
    }

    const QJsonObject unified = readJsonObject(
        resolveArtifactPath(QStringLiteral(".runtime/unified-sim/final_convergence_report.json")));
    signals.unified_sim_pass = unified.value(QStringLiteral("global_convergence")).toBool(false);

    const QString divergence_path = resolveArtifactPath(
        QStringLiteral(".runtime/unified-sim/divergence_events.jsonl"));
    signals.divergence_events_empty = divergence_path.isEmpty() || countJsonlLines(divergence_path) == 0;

    return signals;
}

Evaluation Engine::evaluate(ClientModel* clientModel, WalletModel* walletModel)
{
    const ArtifactSignals artifacts = loadArtifacts();
    const RigorWallet::NodeStatusSnapshot node = RigorWallet::DiagnosticsCollector::collectNode(clientModel);
    const RigorWallet::WalletStatusSnapshot wallet = RigorWallet::DiagnosticsCollector::collectWallet(walletModel);
    const RigorWallet::ExplorerStatusSnapshot explorer =
        RigorWallet::DiagnosticsCollector::collectExplorer(clientModel);
    const RigorWallet::ConvergenceDiagnosticsSnapshot convergence =
        RigorWallet::DiagnosticsCollector::collectConvergence(clientModel);
    const RigorWallet::VerificationStatusSnapshot verification =
        RigorWallet::DiagnosticsCollector::collectVerification(clientModel);
    return evaluateFromSignals(artifacts, node, wallet, explorer, convergence, verification);
}

Evaluation Engine::evaluateFromSignals(
    const ArtifactSignals& artifacts,
    const RigorWallet::NodeStatusSnapshot& node,
    const RigorWallet::WalletStatusSnapshot& wallet,
    const RigorWallet::ExplorerStatusSnapshot& explorer,
    const RigorWallet::ConvergenceDiagnosticsSnapshot& convergence,
    const RigorWallet::VerificationStatusSnapshot& verification)
{
    Evaluation eval;
    eval.evaluated_at_utc = utcNow();
    eval.current_level = Level::G0_Development;
    eval.highest_satisfied_level = Level::G0_Development;

    auto advance = [&](Level next) {
        eval.current_level = next;
        eval.highest_satisfied_level = next;
    };

    if (!checkG1(artifacts, eval)) {
        eval.progress_percent = 16;
        eval.readiness_score = 16;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G1_UiComplete);

    if (!checkG2(artifacts, eval)) {
        eval.progress_percent = 33;
        eval.readiness_score = 33;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G2_SimulationValidated);

    if (!checkG3(artifacts, eval)) {
        eval.progress_percent = 50;
        eval.readiness_score = 50;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G3_RuntimeValidated);

    if (!checkG4(artifacts, node, wallet, eval)) {
        eval.progress_percent = 66;
        eval.readiness_score = 66;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G4_ProductReady);

    if (!checkG5(artifacts, explorer, convergence, eval)) {
        eval.progress_percent = 83;
        eval.readiness_score = 83;
        eval.testnet_release_ready = false;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G5_TestnetCandidate);
    eval.testnet_release_ready = true;

    if (!checkG6(artifacts, verification, eval)) {
        eval.progress_percent = 95;
        eval.readiness_score = 95;
        eval.next_required_action = deriveNextAction(eval);
        return eval;
    }
    advance(Level::G6_MainnetCandidate);
    eval.progress_percent = 100;
    eval.readiness_score = 100;
    eval.next_required_action = deriveNextAction(eval);
    return eval;
}

bool Engine::writeSnapshots(const Evaluation& eval,
                            const ArtifactSignals& artifacts,
                            const QString& output_dir,
                            QString* error_out)
{
    QDir dir(output_dir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error_out) *error_out = QCoreApplication::translate("ReleaseGate", "Could not create output directory.");
        return false;
    }

    auto writeFile = [&](const QString& name, const QJsonObject& obj) -> bool {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error_out) *error_out = file.errorString();
            return false;
        }
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        return true;
    };

    QJsonObject state;
    state.insert(QStringLiteral("task"), QStringLiteral(""));
    state.insert(QStringLiteral("evaluated_at"), eval.evaluated_at_utc);
    state.insert(QStringLiteral("current_gate"), levelShortId(eval.current_level));
    state.insert(QStringLiteral("current_gate_label"), levelLabel(eval.current_level));
    state.insert(QStringLiteral("highest_satisfied_gate"), levelShortId(eval.highest_satisfied_level));
    state.insert(QStringLiteral("testnet_release_ready"), eval.testnet_release_ready);
    state.insert(QStringLiteral("equivalence_claimed"), false);
    state.insert(QStringLiteral("consensus_impact"), QStringLiteral("NONE"));
    state.insert(QStringLiteral("protocol_impact"), QStringLiteral("NONE"));
    state.insert(QStringLiteral("mining_impact"), QStringLiteral("NONE"));
    state.insert(QStringLiteral("proof034_impact"), QStringLiteral("NONE"));
    if (!writeFile(QStringLiteral("release_gate_state.json"), state)) return false;

    QJsonArray blockers;
    for (const Blocker& b : eval.blockers) {
        QJsonObject row;
        row.insert(QStringLiteral("gate"), b.gate_id);
        row.insert(QStringLiteral("code"), b.code);
        row.insert(QStringLiteral("message"), b.message);
        blockers.append(row);
    }
    QJsonObject blockers_root;
    blockers_root.insert(QStringLiteral("blocker_count"), blockers.size());
    blockers_root.insert(QStringLiteral("blockers"), blockers);
    blockers_root.insert(QStringLiteral("next_required_action"), eval.next_required_action);
    if (!writeFile(QStringLiteral("release_blockers.json"), blockers_root)) return false;

    QJsonObject metrics;
    metrics.insert(QStringLiteral("progress_percent"), eval.progress_percent);
    metrics.insert(QStringLiteral("readiness_score"), eval.readiness_score);
    metrics.insert(QStringLiteral("ui_release_candidate_ready"), artifacts.ui_release_candidate_ready);
    metrics.insert(QStringLiteral("wallet_runtime_manifest_pass"), artifacts.wallet_runtime_manifest_pass);
    metrics.insert(QStringLiteral("wallet_sim_pass"), artifacts.wallet_sim_pass);
    metrics.insert(QStringLiteral("gui_replay_consistent"), artifacts.gui_replay_consistent);
    metrics.insert(QStringLiteral("unified_sim_pass"), artifacts.unified_sim_pass);
    metrics.insert(QStringLiteral("divergence_events_empty"), artifacts.divergence_events_empty);
    metrics.insert(QStringLiteral("state_authority_docs_present"), artifacts.state_authority_docs_present);
    metrics.insert(QStringLiteral("equivalence_claimed_false"), artifacts.equivalence_claimed_false);
    if (!writeFile(QStringLiteral("release_metrics.json"), metrics)) return false;

    return true;
}

} // namespace ReleaseGate
