// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_convergencepanel.h>

#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/convergence/convergence_education.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/diagnostics/diagnostics_collector.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <consensus/rigor_consensus_lock.h>
#include <interfaces/convergence_block_result.h>
#include <interfaces/node.h>
#include <uint256.h>
#include <wallet/convergence_window_ui.h>

#include <univalue.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString truncateHash(const QString& hex)
{
    if (hex.isEmpty()) return QString();
    if (hex.size() <= 24) return hex;
    return hex.left(12) + QStringLiteral("\u2026") + hex.right(12);
}

QString WinnerDisplay(const uint256& w)
{
    if (w.IsNull()) return QObject::tr("None (no unique winner)");
    return truncateHash(QString::fromStdString(w.GetHex()));
}
} // namespace

DiagnosticsConvergencePanel::DiagnosticsConvergencePanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorDiagnosticsConvergencePanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral("QFrame#RigorDiagnosticsConvergencePanel { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface), RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(tr("Convergence Details"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_badge = new StatusBadge(this);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_badge);
    layout->addLayout(header);

    auto* summaryTitle = new QLabel(tr("Summary"), this);
    summaryTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 4px;"));
    layout->addWidget(summaryTitle);
    auto* summary = new QFormLayout();
    m_pendingLabel = new QLabel(this);
    m_completedLabel = new QLabel(this);
    m_mintLabel = new QLabel(this);
    m_lagLabel = new QLabel(this);
    summary->addRow(tr("Pending convergence:"), m_pendingLabel);
    summary->addRow(tr("Latest completed convergence:"), m_completedLabel);
    summary->addRow(tr("Latest mint:"), m_mintLabel);
    summary->addRow(tr("Mint lag (blocks):"), m_lagLabel);
    layout->addLayout(summary);

    auto* consensusTitle = new QLabel(tr("Consensus Information"), this);
    consensusTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px;"));
    layout->addWidget(consensusTitle);
    auto* consensus = new QFormLayout();
    m_windowWLabel = new QLabel(this);
    m_selectionTargetLabel = new QLabel(this);
    m_selectionTargetLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_resTotalLabel = new QLabel(this);
    m_windowProgressLabel = new QLabel(this);
    m_convergenceSumLabel = new QLabel(this);
    m_lastWinnerLabel = new QLabel(this);
    m_lastWinnerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_tipHeightLabel = new QLabel(this);
    m_lastResHeightLabel = new QLabel(this);
    consensus->addRow(tr("Consensus window W:"), m_windowWLabel);
    consensus->addRow(tr("Selection target:"), m_selectionTargetLabel);
    consensus->addRow(tr("RES total (minted):"), m_resTotalLabel);
    consensus->addRow(tr("Window progress:"), m_windowProgressLabel);
    consensus->addRow(tr("Convergence since last RES:"), m_convergenceSumLabel);
    consensus->addRow(tr("Last winner txid:"), m_lastWinnerLabel);
    consensus->addRow(tr("Tip height:"), m_tipHeightLabel);
    consensus->addRow(tr("Last RES height:"), m_lastResHeightLabel);
    layout->addLayout(consensus);

    auto* roundTitle = new QLabel(tr("Round / Candidate Diagnostics"), this);
    roundTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px;"));
    layout->addWidget(roundTitle);
    m_tipRoundLabel = new QLabel(this);
    m_tipRoundLabel->setWordWrap(true);
    m_tipRoundLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_prevRoundLabel = new QLabel(this);
    m_prevRoundLabel->setWordWrap(true);
    m_prevRoundLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_tipRoundLabel);
    layout->addWidget(m_prevRoundLabel);

    auto* drawTitle = new QLabel(tr("Draw Bind / Digests"), this);
    drawTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px;"));
    layout->addWidget(drawTitle);
    auto* draw = new QFormLayout();
    m_drawNextLabel = new QLabel(this);
    m_drawBindHeightLabel = new QLabel(this);
    m_drawBindHashLabel = new QLabel(this);
    m_drawBindHashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    draw->addRow(tr("Next block height:"), m_drawNextLabel);
    draw->addRow(tr("Bind block height:"), m_drawBindHeightLabel);
    draw->addRow(tr("Bind block hash:"), m_drawBindHashLabel);
    layout->addLayout(draw);

    auto* debugTitle = new QLabel(tr("Debug Information"), this);
    debugTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px;"));
    layout->addWidget(debugTitle);
    auto* debug = new QFormLayout();
    m_peersLabel = new QLabel(this);
    m_netHeightLabel = new QLabel(this);
    m_syncLabel = new QLabel(this);
    m_historicalCountLabel = new QLabel(this);
    m_latestTxLabel = new QLabel(this);
    m_latestTxLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    debug->addRow(tr("Peers:"), m_peersLabel);
    debug->addRow(tr("Chain height:"), m_netHeightLabel);
    debug->addRow(tr("Sync:"), m_syncLabel);
    debug->addRow(tr("Historical convergence count:"), m_historicalCountLabel);
    debug->addRow(tr("Latest convergence txid:"), m_latestTxLabel);
    debug->addRow(tr("Validation / notes:"), m_validationLabel);
    layout->addLayout(debug);

    auto* eduTitle = new QLabel(tr("Protocol Education (reference)"), this);
    eduTitle->setStyleSheet(QStringLiteral("font-weight: 600; margin-top: 8px; color: %1;")
                                .arg(RigorTheme::colorHex(p.textPrimary)));
    layout->addWidget(eduTitle);
    m_educationView = new QTextEdit(this);
    m_educationView->setObjectName(QStringLiteral("diagnosticsEducationView"));
    m_educationView->setReadOnly(true);
    m_educationView->setFrameShape(QFrame::NoFrame);
    m_educationView->setMaximumHeight(180);
    // Match Convergence Details panel surface (no default white QTextEdit chrome).
    m_educationView->setStyleSheet(QStringLiteral(
                                       "QTextEdit#diagnosticsEducationView {"
                                       "  background: %1;"
                                       "  color: %2;"
                                       "  border: 1px solid %3;"
                                       "  border-radius: 4px;"
                                       "  padding: 6px;"
                                       "}")
                                       .arg(RigorTheme::colorHex(p.bgSurface),
                                            RigorTheme::colorHex(p.textPrimary),
                                            RigorTheme::colorHex(p.borderSubtle)));
    m_educationView->document()->setDefaultStyleSheet(
        QStringLiteral("body { color: %1; background-color: %2; }"
                       "h3 { color: %1; }"
                       "p { color: %3; }")
            .arg(RigorTheme::colorHex(p.textPrimary),
                 RigorTheme::colorHex(p.bgSurface),
                 RigorTheme::colorHex(p.textMuted)));
    m_educationView->setHtml(ConvergenceEducation::fullHelpHtml());
    layout->addWidget(m_educationView);
}

void DiagnosticsConvergencePanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void DiagnosticsConvergencePanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void DiagnosticsConvergencePanel::refresh()
{
    const ConvergenceDiagnosticsSnapshot snap =
        DiagnosticsCollector::collectConvergence(m_clientModel, m_walletModel);
    m_badge->setStatus(snap.overall, StatusBadge::labelForColor(snap.overall));
    m_pendingLabel->setText(snap.pending ? truncateHash(snap.pending_txid) : tr("None"));
    if (snap.pending) m_pendingLabel->setToolTip(snap.pending_txid);
    m_completedLabel->setText(snap.latest_completed_txid.isEmpty()
                                  ? tr("None")
                                  : truncateHash(snap.latest_completed_txid));
    if (!snap.latest_completed_txid.isEmpty()) m_completedLabel->setToolTip(snap.latest_completed_txid);
    m_mintLabel->setText(snap.latest_mint_txid.isEmpty() ? tr("None") : truncateHash(snap.latest_mint_txid));
    if (!snap.latest_mint_txid.isEmpty()) m_mintLabel->setToolTip(snap.latest_mint_txid);
    m_lagLabel->setText(snap.mint_lag >= 0 ? QString::number(snap.mint_lag) : tr("—"));

    m_windowWLabel->setText(QString::number(Consensus::CONVERGENCE_DRAW_WINDOW));

    // Selection target + market / getconvergencestatus
    QString target_display = tr("unavailable");
    try {
        if (m_clientModel) {
            UniValue chain = m_clientModel->node().executeRpc("getblockchaininfo", UniValue(UniValue::VARR), "/");
            if (chain.isObject() && chain.exists("res_probability_target")) {
                target_display = QString::fromStdString(chain["res_probability_target"].get_str());
            }
        }
    } catch (...) {
    }
    m_selectionTargetLabel->setText(target_display);
    m_selectionTargetLabel->setToolTip(target_display);

    auto dash_market = [this]() {
        m_resTotalLabel->setText(tr("—"));
        m_windowProgressLabel->setText(tr("—"));
        m_convergenceSumLabel->setText(tr("—"));
        m_lastWinnerLabel->setText(tr("—"));
        m_tipHeightLabel->setText(tr("—"));
        m_lastResHeightLabel->setText(tr("—"));
    };
    if (!m_clientModel) {
        dash_market();
    } else {
        try {
            UniValue r = m_clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
            if (!r.isObject()) {
                dash_market();
            } else {
                m_resTotalLabel->setText(QString::fromStdString(r["res_total"].getValStr()));
                const std::string wp = r["window_progress"].getValStr();
                const double wpp = r["window_progress_percent"].get_real();
                m_windowProgressLabel->setText(QStringLiteral("%1 (%2%)")
                                                   .arg(QString::fromStdString(wp))
                                                   .arg(wpp, 0, 'f', 2));
                m_convergenceSumLabel->setText(QString::fromStdString(r["current_convergence_sum"].getValStr()));
                const QString win_hex = QString::fromStdString(r["last_winner_txid"].getValStr());
                const QString win_disp = (win_hex == QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"))
                    ? QStringLiteral("—")
                    : truncateHash(win_hex);
                m_lastWinnerLabel->setText(win_disp);
                m_lastWinnerLabel->setToolTip(win_hex);
                m_tipHeightLabel->setText(QString::number(r["tip_height"].getInt<int>()));
                m_lastResHeightLabel->setText(QString::number(r["last_res_height"].getInt<int>()));
            }
        } catch (...) {
            dash_market();
        }
    }

    // Round diagnostics (tip + tip-1)
    auto apply_round = [&](int height, QLabel* label, const QString& heading) {
        if (!m_clientModel || height < 0) {
            label->setText(tr("%1: —").arg(heading));
            return;
        }
        interfaces::ConvergenceBlockResult r;
        bool got = false;
        if (m_walletModel) {
            got = m_walletModel->tryGetConvergenceBlockResult(height, r);
        } else {
            got = m_clientModel->getConvergenceBlockResult(height, {}, r);
        }
        if (!got || !r.ok) {
            label->setText(tr("%1 (height %2): unavailable — %3")
                               .arg(heading)
                               .arg(height)
                               .arg(r.error_message.empty() ? tr("no data") : QString::fromStdString(r.error_message)));
            return;
        }
        const QString mine = m_walletModel
            ? (r.is_mine ? tr("yes") : tr("no"))
            : tr("—");
        label->setText(tr("%1 — height %2 | |F|=%3 | candidates=%4 | winner=%5 | yours=%6")
                           .arg(heading)
                           .arg(r.height)
                           .arg(r.convergence_root_set_size)
                           .arg(r.candidates_count)
                           .arg(WinnerDisplay(r.winner_txid))
                           .arg(mine));
    };
    const int tip = m_clientModel ? m_clientModel->getNumBlocks() : -1;
    apply_round(tip, m_tipRoundLabel, tr("Chain tip round"));
    apply_round(tip >= 1 ? tip - 1 : -1, m_prevRoundLabel, tr("Previous round"));

    // Draw bind
    if (!m_clientModel) {
        m_drawNextLabel->setText(tr("—"));
        m_drawBindHeightLabel->setText(tr("—"));
        m_drawBindHashLabel->setText(tr("—"));
    } else {
        m_drawNextLabel->setText(QString::number(tip + 1));
        m_drawBindHeightLabel->setText(QString::number(tip));
        QString hash_display = tr("—");
        try {
            UniValue params(UniValue::VARR);
            params.push_back(tip);
            UniValue r = m_clientModel->node().executeRpc("getblockhash", params, "/");
            if (r.isStr()) hash_display = QString::fromStdString(r.get_str());
        } catch (...) {
            hash_display = tr("unavailable");
        }
        m_drawBindHashLabel->setText(truncateHash(hash_display));
        m_drawBindHashLabel->setToolTip(hash_display);
    }

    // Network / wallet debug
    if (!m_clientModel) {
        m_peersLabel->setText(tr("—"));
        m_netHeightLabel->setText(tr("—"));
        m_syncLabel->setText(tr("—"));
    } else {
        m_peersLabel->setText(QString::number(m_clientModel->getNumConnections()));
        m_netHeightLabel->setText(QString::number(tip));
        const bool ibd = m_clientModel->node().isInitialBlockDownload();
        const int headers = m_clientModel->getHeaderTipHeight();
        if (ibd) {
            m_syncLabel->setText(tr("Initial block download…"));
        } else if (headers >= 0 && tip != headers) {
            m_syncLabel->setText(tr("Synchronizing (%1 / %2)").arg(tip).arg(headers));
        } else {
            m_syncLabel->setText(tr("Synced"));
        }
    }

    if (!m_walletModel) {
        m_historicalCountLabel->setText(tr("—"));
        m_latestTxLabel->setText(tr("—"));
        m_validationLabel->setText(tr("No wallet loaded."));
    } else {
        const auto rows = ConvergenceWalletData::buildHistory(m_walletModel, tip);
        m_historicalCountLabel->setText(QString::number(ConvergenceWalletData::historicalCount(rows)));
        if (const ConvergenceHistoryRow* latest = ConvergenceWalletData::latestRow(rows)) {
            m_latestTxLabel->setText(truncateHash(latest->txid));
            m_latestTxLabel->setToolTip(latest->txid);
            m_validationLabel->setText(
                tr("Lifecycle=%1 | verification_pass=%2 | start=%3 | finalization=%4 | remaining=%5")
                    .arg(QString::fromUtf8(wallet::ConvergenceUiLifecycleLabel(latest->lifecycle)))
                    .arg(latest->window.verification_pass ? tr("true") : tr("false"))
                    .arg(latest->window.start_block)
                    .arg(latest->window.finalization_block)
                    .arg(latest->window.remaining_blocks));
        } else {
            m_latestTxLabel->setText(tr("None"));
            m_validationLabel->setText(tr("No convergence entries in wallet history."));
        }
    }
}

} // namespace RigorWallet
