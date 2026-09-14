// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_COLLECTOR_H
#define BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_COLLECTOR_H

#include <qt/rigorwallet/widgets/statusbadge.h>

#include <QString>

class ClientModel;
class WalletModel;

namespace RigorWallet {

struct NodeStatusSnapshot {
    int block_height{-1};
    int headers{-1};
    int peers{0};
    double sync_progress{0.0};
    QString best_block_hash;
    bool initial_block_download{false};
    bool network_active{false};
    StatusBadgeColor overall{StatusBadgeColor::Gray};
};

struct WalletStatusSnapshot {
    bool loaded{false};
    bool encrypted{false};
    bool unlocked{false};
    QString backup_status;
    StatusBadgeColor overall{StatusBadgeColor::Gray};
};

struct ExplorerStatusSnapshot {
    bool connected{false};
    int indexer_lag{-1};
    int latest_indexed_height{-1};
    QString api_base_url;
    StatusBadgeColor overall{StatusBadgeColor::Gray};
};

struct ConvergenceDiagnosticsSnapshot {
    int mint_lag{-1};
    StatusBadgeColor overall{StatusBadgeColor::Gray};
};

struct VerificationStatusSnapshot {
    bool proof034_running{false};
    QString archive_state;
    QString simulation_readiness;
    int proof_current_height{-1};
    int proof_target_height{2100001};
    double proof_progress_pct{0.0};
    bool equivalence_claimed{false};
    StatusBadgeColor overall{StatusBadgeColor::Gray};
};

/** Read-only collectors for diagnostics panels and export (BUILD-27–34). */
class DiagnosticsCollector
{
public:
    DiagnosticsCollector() = delete;

    static NodeStatusSnapshot collectNode(ClientModel* clientModel);
    static WalletStatusSnapshot collectWallet(WalletModel* walletModel);
    static ExplorerStatusSnapshot collectExplorer(ClientModel* clientModel);
    static ConvergenceDiagnosticsSnapshot collectConvergence(ClientModel* clientModel);
    static VerificationStatusSnapshot collectVerification(ClientModel* clientModel);

    static QString nodeStatusJson(const NodeStatusSnapshot& snap);
    static QString walletStatusJson(const WalletStatusSnapshot& snap);
    static QString convergenceStatusJson(const ConvergenceDiagnosticsSnapshot& snap);
    static QString verificationStatusJson(const VerificationStatusSnapshot& snap);
    static QString explorerStatusJson(ClientModel* clientModel);
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_COLLECTOR_H
