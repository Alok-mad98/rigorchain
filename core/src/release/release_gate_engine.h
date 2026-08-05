// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RELEASE_RELEASE_GATE_ENGINE_H
#define BITCOIN_RELEASE_RELEASE_GATE_ENGINE_H

#include <QString>
#include <QStringList>
#include <QVector>

class ClientModel;
class WalletModel;

namespace RigorWallet {
struct NodeStatusSnapshot;
struct WalletStatusSnapshot;
struct ExplorerStatusSnapshot;
struct ConvergenceDiagnosticsSnapshot;
struct VerificationStatusSnapshot;
}

namespace ReleaseGate {

/** Release gate levels — TASK-WALLET-PROGRAM-018. */
enum class Level {
    G0_Development = 0,
    G1_UiComplete = 1,
    G2_SimulationValidated = 2,
    G3_RuntimeValidated = 3,
    G4_ProductReady = 4,
    G5_TestnetCandidate = 5,
    G6_MainnetCandidate = 6,
};

struct Blocker {
    QString gate_id;
    QString code;
    QString message;
};

/** File-backed artifact signals (PROGRAM-010–017, SIM-011, SIM-UNIFIED). */
struct ArtifactSignals {
    bool ui_release_candidate_ready{false};
    bool ui_finalization_report_present{false};
    bool state_authority_docs_present{false};
    bool wallet_runtime_manifest_pass{false};
    bool wallet_sim_pass{false};
    bool gui_replay_consistent{false};
    bool unified_sim_pass{false};
    bool divergence_events_empty{true};
    bool equivalence_claimed_false{true};
};

struct Evaluation {
    Level current_level{Level::G0_Development};
    Level highest_satisfied_level{Level::G0_Development};
    bool testnet_release_ready{false};
    int progress_percent{0};
    int readiness_score{0};
    QVector<Blocker> blockers;
    QString next_required_action;
    QString evaluated_at_utc;
};

/** Read-only release gate evaluator — no side effects except explicit snapshot export. */
class Engine
{
public:
    Engine() = delete;

    static QString levelLabel(Level level);
    static QString levelShortId(Level level);

    static QStringList searchRoots();
    static QString resolveArtifactPath(const QString& relative_path);

    static ArtifactSignals loadArtifacts();

    static Evaluation evaluate(ClientModel* clientModel, WalletModel* walletModel);

    static Evaluation evaluateFromSignals(
        const ArtifactSignals& artifacts,
        const RigorWallet::NodeStatusSnapshot& node,
        const RigorWallet::WalletStatusSnapshot& wallet,
        const RigorWallet::ExplorerStatusSnapshot& explorer,
        const RigorWallet::ConvergenceDiagnosticsSnapshot& convergence,
        const RigorWallet::VerificationStatusSnapshot& verification);

    static bool writeSnapshots(const Evaluation& eval,
                               const ArtifactSignals& artifacts,
                               const QString& output_dir,
                               QString* error_out = nullptr);
};

} // namespace ReleaseGate

#endif // BITCOIN_RELEASE_RELEASE_GATE_ENGINE_H
