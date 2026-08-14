// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/convergence/convergence_wallet_data.h>

#include <qt/walletmodel.h>

#include <consensus/convergenceres.h>
#include <consensus/rigor_consensus_lock.h>
#include <wallet/tx_semantic.h>

#include <uint256.h>

#include <algorithm>
#include <map>
#include <set>

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QtGlobal>

namespace RigorWallet {

ConvergenceUserStatus ConvergenceWalletData::mapStateString(const QString& state)
{
    if (state == QLatin1String("BROADCASTED") || state == QLatin1String("CREATED")) {
        return ConvergenceUserStatus::Pending;
    }
    if (state == QLatin1String("INCLUDED") || state == QLatin1String("MATURING")) {
        return ConvergenceUserStatus::Included;
    }
    if (state == QLatin1String("LOST")) {
        return ConvergenceUserStatus::Completed;
    }
    if (state == QLatin1String("WON_PENDING") || state == QLatin1String("WON_CONFIRMED")) {
        return ConvergenceUserStatus::Minted;
    }
    return ConvergenceUserStatus::Failed;
}

QString ConvergenceWalletData::statusLabel(ConvergenceUserStatus status)
{
    switch (status) {
    case ConvergenceUserStatus::Pending:
        return QCoreApplication::translate("ConvergenceWalletData", "Submitted");
    case ConvergenceUserStatus::Included:
        return QCoreApplication::translate("ConvergenceWalletData", "Active window");
    case ConvergenceUserStatus::Completed:
        return QCoreApplication::translate("ConvergenceWalletData", "No RES mint");
    case ConvergenceUserStatus::Minted:
        return QCoreApplication::translate("ConvergenceWalletData", "RES minted");
    case ConvergenceUserStatus::Failed:
        return QCoreApplication::translate("ConvergenceWalletData", "Failed");
    }
    return QCoreApplication::translate("ConvergenceWalletData", "Unknown");
}

static bool isConvergenceWalletTx(const interfaces::WalletTx& wtx)
{
    if (wtx.semantic == wallet::TxSemanticKind::OruConvergence) return true;
    const auto it = wtx.value_map.find("convergence_v2");
    return it != wtx.value_map.end() && it->second == "1";
}

static QString mapValueStr(const interfaces::WalletTx& wtx, const char* key)
{
    const auto it = wtx.value_map.find(key);
    return it != wtx.value_map.end() ? QString::fromStdString(it->second) : QString();
}

static int mintBlockHeight(WalletModel* walletModel, const uint256& txid)
{
    interfaces::WalletTxStatus status;
    interfaces::WalletOrderForm order;
    bool in_mempool = false;
    int num_blocks = 0;
    walletModel->wallet().getWalletTxDetails(txid, status, order, in_mempool, num_blocks);
    if (status.block_height >= 0) return status.block_height;
    return -1;
}

std::vector<ConvergenceHistoryRow> ConvergenceWalletData::buildHistory(WalletModel* walletModel, int tip_height)
{
    std::vector<ConvergenceHistoryRow> rows;
    if (!walletModel) return rows;

    // Coalesce repeated Convergence panel refreshes (lifecycle/history/overview/warnings)
    // that previously each rescanned the wallet on the GUI thread.
    static WalletModel* s_cached_wallet = nullptr;
    static int s_cached_tip = -2;
    static uint64_t s_cached_rev = 0;
    static std::vector<ConvergenceHistoryRow> s_cache;
    const uint64_t rev = walletModel->wallet().getWalletStateRevision();
    if (s_cached_wallet == walletModel && s_cached_tip == tip_height && s_cached_rev == rev) {
        return s_cache;
    }

    QElapsedTimer build_timer;
    build_timer.start();

    std::map<int, QString> mint_by_height;
    const std::set<interfaces::WalletTx> txs = walletModel->wallet().getWalletTxs();
    for (const interfaces::WalletTx& wtx : txs) {
        if (wtx.semantic != wallet::TxSemanticKind::ResMint) continue;
        const uint256 hash = wtx.tx->GetHash();
        const int h = mintBlockHeight(walletModel, hash);
        if (h >= 0) {
            mint_by_height[h] = QString::fromStdString(hash.GetHex());
        }
    }

    std::vector<QString> row_states;
    for (const interfaces::WalletTx& wtx : txs) {
        if (!isConvergenceWalletTx(wtx)) continue;
        ConvergenceHistoryRow row;
        row.txid = QString::fromStdString(wtx.tx->GetHash().GetHex());
        row.timestamp = wtx.time;
        // Invested ORU = convergence output value (not -debit: debit_oru is already >= 0).
        row.oru_amount = Consensus::GetConvergenceAmount(*wtx.tx);
        if (row.oru_amount <= 0) {
            const CAmount net_out = wtx.debit_oru - wtx.credit_oru;
            row.oru_amount = net_out > 0 ? net_out : wtx.debit_oru;
        }
        const QString state = mapValueStr(wtx, "convergence_state");
        row.status = state.isEmpty() ? ConvergenceUserStatus::Pending : mapStateString(state);
        row.status_label = statusLabel(row.status);
        const QString inc = mapValueStr(wtx, "convergence_included_height");
        if (!inc.isEmpty()) row.block_height = inc.toInt();
        row_states.push_back(state);
        rows.push_back(std::move(row));
    }

    // Each body RES mint credits exactly one winner. Overlapping W-windows must not all claim
    // the same mint height (that double-counted Success/100 in history while balance stayed 100).
    for (const auto& mint_kv : mint_by_height) {
        const int mint_h = mint_kv.first;
        const QString& mint_txid = mint_kv.second;
        int best = -1;
        for (size_t i = 0; i < rows.size(); ++i) {
            const ConvergenceHistoryRow& cand = rows[i];
            if (cand.block_height < 0) continue;
            if (mint_h < cand.block_height || mint_h > cand.block_height + Consensus::CONVERGENCE_DRAW_WINDOW) {
                continue;
            }
            if (cand.res_amount > 0) continue; // already claimed another mint
            if (best < 0) {
                best = static_cast<int>(i);
                continue;
            }
            const ConvergenceHistoryRow& cur = rows[best];
            const bool cand_won = (cand.status == ConvergenceUserStatus::Minted);
            const bool cur_won = (cur.status == ConvergenceUserStatus::Minted);
            if (cand_won != cur_won) {
                if (cand_won) best = static_cast<int>(i);
                continue;
            }
            // Prefer the larger Convergence stake when wallet state does not distinguish.
            if (cand.oru_amount > cur.oru_amount) {
                best = static_cast<int>(i);
            } else if (cand.oru_amount == cur.oru_amount && cand.block_height < cur.block_height) {
                best = static_cast<int>(i);
            }
        }
        if (best < 0) continue;
        ConvergenceHistoryRow& row = rows[best];
        row.linked_mint_txid = mint_txid;
        row.result_block = mint_h;
        row.res_amount = Consensus::RES_REWARD_AMOUNT;
        row.status = ConvergenceUserStatus::Minted;
        row.status_label = statusLabel(row.status);
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        ConvergenceHistoryRow& row = rows[i];
        const QString& state = row_states[i];
        const bool allocated = (row.status == ConvergenceUserStatus::Minted) || (row.res_amount > 0);
        // Only treat LOST as final once the W-window has closed (guards premature wallet LOST).
        const bool explicit_loss =
            (state == QLatin1String("LOST")) && row.block_height >= 0 &&
            tip_height >= row.block_height + Consensus::CONVERGENCE_DRAW_WINDOW && !allocated;

        const bool won = allocated;
        row.window = wallet::BuildConvergenceUiWindow(row.block_height, tip_height, won, explicit_loss);
        // While still in-window, override a premature LOST status label for the lifecycle row.
        if (!won && !explicit_loss && state == QLatin1String("LOST") &&
            (row.window.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow ||
             row.window.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending)) {
            row.status = ConvergenceUserStatus::Included;
            row.status_label = statusLabel(row.status);
        }
        row.lifecycle = row.window.lifecycle;
        // Prefer lifecycle-derived label for active tracking.
        if (row.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow ||
            row.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending) {
            row.status_label = QString::fromUtf8(wallet::ConvergenceUiLifecycleLabel(row.lifecycle));
        }
    }

    std::sort(rows.begin(), rows.end(), [](const ConvergenceHistoryRow& a, const ConvergenceHistoryRow& b) {
        return a.timestamp > b.timestamp;
    });

    s_cached_wallet = walletModel;
    s_cached_tip = tip_height;
    s_cached_rev = rev;
    s_cache = rows;
    qInfo().noquote() << QStringLiteral("[ui-perf] ConvergenceWalletData.buildHistory %1ms rows=%2 tip=%3")
                             .arg(build_timer.elapsed())
                             .arg(static_cast<qulonglong>(rows.size()))
                             .arg(tip_height);
    return rows;
}

int ConvergenceWalletData::countByStatus(const std::vector<ConvergenceHistoryRow>& rows, ConvergenceUserStatus status)
{
    return static_cast<int>(std::count_if(rows.begin(), rows.end(), [status](const ConvergenceHistoryRow& r) {
        return r.status == status;
    }));
}

int ConvergenceWalletData::historicalCount(const std::vector<ConvergenceHistoryRow>& rows)
{
    return static_cast<int>(rows.size());
}

const ConvergenceHistoryRow* ConvergenceWalletData::latestRow(const std::vector<ConvergenceHistoryRow>& rows)
{
    return rows.empty() ? nullptr : &rows.front();
}

const ConvergenceHistoryRow* ConvergenceWalletData::pendingRow(const std::vector<ConvergenceHistoryRow>& rows)
{
    for (const ConvergenceHistoryRow& r : rows) {
        if (r.status == ConvergenceUserStatus::Pending) return &r;
        if (r.lifecycle == wallet::ConvergenceUiLifecycle::Submitted ||
            r.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow ||
            r.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending) {
            return &r;
        }
    }
    return nullptr;
}

} // namespace RigorWallet
