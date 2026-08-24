// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/convergencepage.h>
#include <qt/forms/ui_convergencepage.h>

#include <qt/rigorwallet/widgets/convergence_educationpanel.h>
#include <qt/rigorwallet/widgets/convergence_overviewpanel.h>
#include <qt/rigorwallet/widgets/convergence_lifecyclepanel.h>
#include <qt/rigorwallet/widgets/convergence_startpanel.h>
#include <qt/rigorwallet/widgets/convergence_networkpanel.h>
#include <qt/rigorwallet/widgets/convergence_resprobabilitypanel.h>
#include <qt/rigorwallet/widgets/convergence_historypanel.h>
#include <qt/rigorwallet/widgets/warningbanner.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <interfaces/convergence_block_result.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>

#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h> // SyncType, SynchronizationState
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <univalue.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QElapsedTimer>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QString>
#include <QTimer>
#include <QtGlobal>

#include <algorithm>
#include <string>

namespace {
QString TruncateMiddleHex(const QString& hex, int head = 10, int tail = 10)
{
    if (hex.size() <= head + tail + 1) return hex;
    return hex.left(head) + QStringLiteral("\u2026") + hex.right(tail);
}

QString WinnerDisplay(const uint256& w)
{
    if (w.IsNull()) {
        return QObject::tr("None (no unique winner)");
    }
    return TruncateMiddleHex(QString::fromStdString(w.GetHex()));
}

void ClearHighlight(QLabel* label)
{
    if (!label) return;
    label->setStyleSheet(QString());
}
} // namespace

ConvergencePage::ConvergencePage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ConvergencePage),
      m_platform_style(platformStyle)
{
    ui->setupUi(this);

    applyConvergenceTheme();

    // Legacy .ui spend form is replaced by ConvergenceStartPanel (Bitcoin-simple UX).
    if (ui->frameConverge) ui->frameConverge->setVisible(false);
    if (ui->labelConvergeDisclaimer) ui->labelConvergeDisclaimer->setVisible(false);
    connect(ui->pushButtonConverge, &QPushButton::clicked, this, &ConvergencePage::onSendConvergenceClicked);
    connect(ui->convergeAmount, &BitcoinAmountField::valueChanged, this, &ConvergencePage::updateQuadraticFeeHint);

    // User Convergence page: My Convergence → Network → warnings → Start → history.
    m_educationPanel = new RigorWallet::ConvergenceEducationPanel(this);
    m_educationPanel->setVisible(false);
    m_lifecyclePanel = new RigorWallet::ConvergenceLifecyclePanel(this);
    m_networkPanel = new RigorWallet::ConvergenceNetworkPanel(this);
    m_startPanel = new RigorWallet::ConvergenceStartPanel(this);
    m_overviewPanel = new RigorWallet::ConvergenceOverviewPanel(this);
    m_overviewPanel->setVisible(false);
    m_resProbabilityPanel = new RigorWallet::ConvergenceResProbabilityPanel(this);
    m_resProbabilityPanel->setVisible(false);
    m_warningBanner = new RigorWallet::WarningBanner(this);
    m_historyPanel = new RigorWallet::ConvergenceHistoryPanel(this);

    if (ui->verticalLayoutConvergeScroll) {
        ui->verticalLayoutConvergeScroll->insertWidget(0, m_lifecyclePanel);
        ui->verticalLayoutConvergeScroll->insertWidget(1, m_networkPanel);
        ui->verticalLayoutConvergeScroll->insertWidget(2, m_warningBanner);
        ui->verticalLayoutConvergeScroll->insertWidget(3, m_startPanel);
        ui->verticalLayoutConvergeScroll->addWidget(m_historyPanel);
        ui->verticalLayoutConvergeScroll->addWidget(m_educationPanel);
        ui->verticalLayoutConvergeScroll->addWidget(m_overviewPanel);
        ui->verticalLayoutConvergeScroll->addWidget(m_resProbabilityPanel);
    }

    // Hide explorer-style / protocol frames from the user Convergence page.
    if (ui->frameNetwork) ui->frameNetwork->setVisible(false);
    if (ui->frameRoundTable) ui->frameRoundTable->setVisible(false);
    if (ui->frameDraw) ui->frameDraw->setVisible(false);
    if (ui->frameMarket) ui->frameMarket->setVisible(false);

    m_refreshDebounce = new QTimer(this);
    m_refreshDebounce->setSingleShot(true);
    // Event-driven tip/wallet signals already coalesce; keep debounce Core-like (not sub-second thrash).
    m_refreshDebounce->setInterval(1500);
    connect(m_refreshDebounce, &QTimer::timeout, this, &ConvergencePage::refreshVisibleUi);

    connect(m_startPanel, &RigorWallet::ConvergenceStartPanel::convergenceSubmitted, this, [this]() {
        refreshVisibleUi();
    });
}

void ConvergencePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleUiRefresh();
}

void ConvergencePage::scheduleUiRefresh()
{
    if (!isVisible()) return;
    if (!m_refreshDebounce) {
        refreshVisibleUi();
        return;
    }
    m_refreshDebounce->start();
}

void ConvergencePage::refreshVisibleUi()
{
    if (!isVisible()) return;
    QElapsedTimer page_timer;
    page_timer.start();

    // User page: only visible panels. Hidden protocol frames (Network/Round/Market/Draw)
    // stay compiled for Diagnostics but must not run sync RPC/disk on every tip.
    if (ui->frameNetwork && ui->frameNetwork->isVisible()) {
        updateContextPanels();
    }
    if (ui->frameRoundTable && ui->frameRoundTable->isVisible()) {
        updateConvergenceRoundPanels();
    }
    if (ui->frameMarket && ui->frameMarket->isVisible()) {
        updateMarketPanel();
    }
    if (ui->frameConverge && ui->frameConverge->isVisible()) {
        updateMinConvergenceDisplay();
        updateConvergenceStatusDisplay();
    }
    updateConvergenceNetworkSafety();
    refreshPhaseCPanels();

    qInfo().noquote() << QStringLiteral("[ui-perf] ConvergencePage.refreshVisibleUi %1ms")
                             .arg(page_timer.elapsed());
}

ConvergencePage::~ConvergencePage()
{
    delete ui;
}

void ConvergencePage::applyConvergenceTheme()
{
    const RigorWallet::ColorPalette p = RigorWallet::RigorTheme::colors();
    const QString bg = RigorWallet::RigorTheme::colorHex(p.bgPrimary);
    const QString surface = RigorWallet::RigorTheme::colorHex(p.bgSurface);
    const QString border = RigorWallet::RigorTheme::colorHex(p.borderSubtle);
    const QString text = RigorWallet::RigorTheme::colorHex(p.textPrimary);
    const QString muted = RigorWallet::RigorTheme::colorHex(p.textMuted);
    const QString critical = RigorWallet::RigorTheme::colorHex(p.critical);

    setObjectName(QStringLiteral("ConvergencePageRoot"));
    setStyleSheet(QStringLiteral(
        "QWidget#ConvergencePageRoot {"
        "  background: %1;"
        "  color: %2;"
        "  font-family: %9;"
        "}"
        "QWidget#ConvergencePageRoot QLabel { color: %2; }"
        "QWidget#ConvergencePageRoot QLabel#labelSectionNetwork,"
        "QWidget#ConvergencePageRoot QLabel#labelSectionRoundTable,"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundSection,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundSection,"
        "QWidget#ConvergencePageRoot QLabel#labelSectionDraw,"
        "QWidget#ConvergencePageRoot QLabel#labelSectionMarket,"
        "QWidget#ConvergencePageRoot QLabel#labelSectionConverge {"
        "  font-weight: 600; font-size: 13px; color: %2;"
        "}"
        "QWidget#ConvergencePageRoot QFrame#frameNetwork,"
        "QWidget#ConvergencePageRoot QFrame#frameRoundTable,"
        "QWidget#ConvergencePageRoot QFrame#frameTipRound,"
        "QWidget#ConvergencePageRoot QFrame#framePrevRound,"
        "QWidget#ConvergencePageRoot QFrame#frameDraw,"
        "QWidget#ConvergencePageRoot QFrame#frameMarket,"
        "QWidget#ConvergencePageRoot QFrame#frameConverge {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %4;"
        "  border-radius: 8px;"
        "}"
        "QWidget#ConvergencePageRoot QScrollArea { background: transparent; border: none; }"
        "QWidget#ConvergencePageRoot QWidget#scrollAreaWidgetContents { background: transparent; }"
        "QWidget#ConvergencePageRoot QLabel#labelNetPeersTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelNetHeightTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelNetSyncTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelDrawNextTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelDrawBindHTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelDrawBindHashTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMinConvergeTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeAmountTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelQuadraticFeeTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelTotalDeductionTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeStatusTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeCompetitionTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeMaturityTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeMaturityCountdownTitle { color: %5; }"
        "QWidget#ConvergencePageRoot QLabel#labelRoundNarrative,"
        "QWidget#ConvergencePageRoot QLabel#labelDrawHint,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeDisclaimer { color: %5; font-size: 11px; }"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundHeightTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundNTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundCandTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundWinnerTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundYouTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundHeightTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundNTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundCandTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundWinnerTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundYouTitle { color: %5; }"
        "QWidget#ConvergencePageRoot QLabel#labelTipRoundError,"
        "QWidget#ConvergencePageRoot QLabel#labelPrevRoundError { color: %6; font-size: 11px; }"
        "QWidget#ConvergencePageRoot QLabel#labelMarketResTotalTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMarketWindowTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMarketMintCostTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMarketWinnerTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMarketTipTitle,"
        "QWidget#ConvergencePageRoot QLabel#labelMarketLastResTitle { color: %5; }"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeUnavailable { color: %2; padding: 12px; background: %3; border: 1px solid %4; border-radius: 8px; }"
        "QWidget#ConvergencePageRoot QPushButton#pushButtonConverge {"
        "  background-color: %7;"
        "  color: #FFFFFF;"
        "  font-weight: 600;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "}"
        "QWidget#ConvergencePageRoot QPushButton#pushButtonConverge:hover { background-color: %8; }"
        "QWidget#ConvergencePageRoot QPushButton#pushButtonConverge:disabled { background-color: %4; color: %5; }"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeMaturity,"
        "QWidget#ConvergencePageRoot QLabel#labelConvergeMaturityCountdown { color: %7; font-weight: 600; }"
        ).arg(bg, text, surface, border, muted, critical,
              RigorWallet::RigorTheme::colorHex(p.accent),
              RigorWallet::RigorTheme::colorHex(p.accentHover),
              GUIUtil::UiFontFamilyCss()));
}

void ConvergencePage::updateConvergenceAvailability()
{
    const bool can_convergence = walletModel && !walletModel->wallet().privateKeysDisabled();
    ui->scrollAreaConverge->setVisible(can_convergence);
    ui->labelConvergeUnavailable->setVisible(!can_convergence);
}

void ConvergencePage::setClientModel(ClientModel* model)
{
    clientModel = model;
    if (model) {
        connect(model, &ClientModel::numBlocksChanged, this, [this](int, const QDateTime&, double, SyncType, SynchronizationState) {
            scheduleUiRefresh();
        });
        connect(model, &ClientModel::numConnectionsChanged, this, [this](int) {
            if (!isVisible()) return;
            // Hidden protocol frames must not run sync RPC on connection flaps.
            if (ui->frameNetwork && ui->frameNetwork->isVisible()) {
                updateContextPanels();
            }
            updateConvergenceNetworkSafety();
        });
        if (model->getOptionsModel()) {
            connect(model->getOptionsModel(), &OptionsModel::useEmbeddedMonospacedFontChanged, this, &ConvergencePage::setMonospacedConvergenceFont);
            setMonospacedConvergenceFont(model->getOptionsModel()->getUseEmbeddedMonospacedFont());
        }
    }
    if (m_resProbabilityPanel) m_resProbabilityPanel->setClientModel(model);
    if (m_lifecyclePanel) m_lifecyclePanel->setClientModel(model);
    if (m_networkPanel) m_networkPanel->setClientModel(model);
    if (m_startPanel) m_startPanel->setClientModel(model);
    if (m_overviewPanel) m_overviewPanel->setClientModel(model);
    if (m_historyPanel) m_historyPanel->setClientModel(model);
    scheduleUiRefresh();
}

void ConvergencePage::setWalletModel(WalletModel* model)
{
    walletModel = model;
    updateConvergenceAvailability();
    if (model && model->getOptionsModel()) {
        // Page amount field is hidden; wizard locks to ORU. Keep field on ORU if ever re-enabled.
        ui->convergeAmount->setDisplayUnit(BitcoinUnit::ORU);
        connect(model, &WalletModel::walletStateRevisionChanged, this, [this](quint64) {
            scheduleUiRefresh();
        });
        connect(model, &WalletModel::balanceSnapshotStaleChanged, this, [this](bool) {
            scheduleUiRefresh();
        });
        connect(model, &WalletModel::notifyWatchonlyChanged, this, [this](bool) {
            updateConvergenceAvailability();
            if (isVisible()) {
                updateQuadraticFeeHint();
                scheduleUiRefresh();
            }
        });
        updateQuadraticFeeHint();
    }
    if (m_lifecyclePanel) m_lifecyclePanel->setWalletModel(model);
    if (m_startPanel) m_startPanel->setWalletModel(model);
    if (m_overviewPanel) m_overviewPanel->setWalletModel(model);
    if (m_historyPanel) m_historyPanel->setWalletModel(model);
    scheduleUiRefresh();
}

void ConvergencePage::setMonospacedConvergenceFont(bool use_embedded_font)
{
    QFont f = GUIUtil::fixedPitchFont(use_embedded_font);
    f.setWeight(QFont::Bold);
    ui->labelMinConverge->setFont(f);
    ui->labelQuadraticFee->setFont(f);
    ui->labelTotalDeduction->setFont(f);
    ui->labelNetHeight->setFont(f);
    ui->labelDrawNextHeight->setFont(f);
    ui->labelDrawBindHeight->setFont(f);
    ui->labelDrawBindHash->setFont(f);
    ui->labelMarketResTotal->setFont(f);
    ui->labelMarketWindow->setFont(f);
    ui->labelMarketMintCost->setFont(f);
    ui->labelMarketWinner->setFont(f);
    ui->labelMarketTip->setFont(f);
    ui->labelMarketLastRes->setFont(f);
    ui->labelTipRoundHeight->setFont(f);
    ui->labelTipRoundN->setFont(f);
    ui->labelTipRoundCand->setFont(f);
    ui->labelTipRoundWinner->setFont(f);
    ui->labelPrevRoundHeight->setFont(f);
    ui->labelPrevRoundN->setFont(f);
    ui->labelPrevRoundCand->setFont(f);
    ui->labelPrevRoundWinner->setFont(f);
}

void ConvergencePage::updateContextPanels()
{
    if (!clientModel) {
        ui->labelNetPeers->setText(tr("—"));
        ui->labelNetHeight->setText(tr("—"));
        ui->labelNetSync->setText(tr("—"));
        ui->labelDrawNextHeight->setText(tr("—"));
        ui->labelDrawBindHeight->setText(tr("—"));
        ui->labelDrawBindHash->setText(tr("—"));
        return;
    }

    const int peers = clientModel->getNumConnections();
    ui->labelNetPeers->setText(QString::number(peers));

    const int tip = clientModel->getNumBlocks();
    ui->labelNetHeight->setText(QString::number(tip));

    const bool ibd = clientModel->node().isInitialBlockDownload();
    const int headers = clientModel->getHeaderTipHeight();
    QString sync_txt;
    if (ibd) {
        sync_txt = tr("Initial block download…");
    } else if (headers >= 0 && tip != headers) {
        sync_txt = tr("Synchronizing (%1 / %2)").arg(tip).arg(headers);
    } else {
        sync_txt = tr("Synced");
    }
    ui->labelNetSync->setText(sync_txt);

    // SECURITY-001-A8: the draw resolved by the next block is seeded by the current tip's hash,
    // so the bind is always the tip rather than an ancestor ten blocks back.
    const int next_h = tip + 1;
    ui->labelDrawNextHeight->setText(QString::number(next_h));
    ui->labelDrawBindHeight->setText(QString::number(tip));

    QString hash_display = tr("—");
    try {
        UniValue params(UniValue::VARR);
        params.push_back(tip);
        UniValue r = clientModel->node().executeRpc("getblockhash", params, "/");
        if (r.isStr()) {
            hash_display = TruncateMiddleHex(QString::fromStdString(r.get_str()));
        }
    } catch (...) {
        hash_display = tr("unavailable");
    }
    ui->labelDrawBindHash->setText(hash_display);
}

void ConvergencePage::updateMarketPanel()
{
    auto dash = [this]() {
        ui->labelMarketResTotal->setText(tr("—"));
        ui->labelMarketWindow->setText(tr("—"));
        ui->labelMarketMintCost->setText(tr("—"));
        ui->labelMarketWinner->setText(tr("—"));
        ui->labelMarketTip->setText(tr("—"));
        ui->labelMarketLastRes->setText(tr("—"));
    };
    if (!clientModel) {
        dash();
        return;
    }
    try {
        UniValue params(UniValue::VARR);
        UniValue r = clientModel->node().executeRpc("getconvergencestatus", params, "/");
        if (!r.isObject()) {
            dash();
            return;
        }
        ui->labelMarketResTotal->setText(QString::fromStdString(r["res_total"].getValStr()));
        const std::string wp = r["window_progress"].getValStr();
        const double wpp = r["window_progress_percent"].get_real();
        ui->labelMarketWindow->setText(QStringLiteral("%1 (%2%)")
            .arg(QString::fromStdString(wp))
            .arg(wpp, 0, 'f', 2));
        ui->labelMarketMintCost->setText(QString::fromStdString(r["current_convergence_sum"].getValStr()));
        const QString win_hex = QString::fromStdString(r["last_winner_txid"].getValStr());
        const QString win_disp = (win_hex == QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"))
            ? QStringLiteral("—")
            : TruncateMiddleHex(win_hex);
        ui->labelMarketWinner->setText(win_disp);
        ui->labelMarketTip->setText(QString::number(r["tip_height"].getInt<int>()));
        ui->labelMarketLastRes->setText(QString::number(r["last_res_height"].getInt<int>()));
    } catch (...) {
        dash();
    }
}

void ConvergencePage::updateConvergenceRoundPanels()
{
    if (!ui->frameRoundTable) return;

    auto dash_tip = [this]() {
        ui->labelTipRoundHeight->setText(tr("—"));
        ui->labelTipRoundN->setText(tr("—"));
        ui->labelTipRoundCand->setText(tr("—"));
        ui->labelTipRoundWinner->setText(tr("—"));
        ui->labelTipRoundYou->setText(tr("—"));
        ClearHighlight(ui->labelTipRoundYou);
        ui->labelTipRoundError->clear();
        ui->labelTipRoundError->setVisible(false);
    };
    auto dash_prev = [this]() {
        ui->labelPrevRoundHeight->setText(tr("—"));
        ui->labelPrevRoundN->setText(tr("—"));
        ui->labelPrevRoundCand->setText(tr("—"));
        ui->labelPrevRoundWinner->setText(tr("—"));
        ui->labelPrevRoundYou->setText(tr("—"));
        ClearHighlight(ui->labelPrevRoundYou);
        ui->labelPrevRoundError->clear();
        ui->labelPrevRoundError->setVisible(false);
    };

    if (!clientModel) {
        dash_tip();
        dash_prev();
        ui->framePrevRound->setVisible(false);
        return;
    }

    const int tip = clientModel->getNumBlocks();

    auto fetch_at = [&](int height, interfaces::ConvergenceBlockResult& out) -> bool {
        if (walletModel) {
            return walletModel->tryGetConvergenceBlockResult(height, out);
        }
        return clientModel->getConvergenceBlockResult(height, {}, out);
    };

    auto apply_row = [&](int height, const interfaces::ConvergenceBlockResult& r, bool got,
                         QLabel* err_label,
                         QLabel* height_l,
                         QLabel* n_l,
                         QLabel* cand_l,
                         QLabel* winner_l,
                         QLabel* you_l) {
        err_label->clear();
        err_label->setVisible(false);
        if (!got || !r.ok) {
            height_l->setText(height >= 0 ? QString::number(height) : tr("—"));
            n_l->setText(tr("—"));
            cand_l->setText(tr("—"));
            winner_l->setText(tr("—"));
            you_l->setText(tr("—"));
            ClearHighlight(you_l);
            const QString msg = r.error_message.empty()
                ? tr("Unable to load convergence data for this block.")
                : QString::fromStdString(r.error_message);
            err_label->setText(msg);
            err_label->setVisible(true);
            return;
        }
        height_l->setText(QString::number(r.height));
        n_l->setText(QString::number(r.convergence_root_set_size));
        cand_l->setText(QString::number(r.candidates_count));
        winner_l->setText(WinnerDisplay(r.winner_txid));
        if (walletModel) {
            if (r.is_mine) {
                you_l->setText(tr("Yes — your convergence transaction is in F for this block."));
                you_l->setStyleSheet(QStringLiteral("QLabel { color: #8CD892; font-weight: 600; }"));
            } else {
                ClearHighlight(you_l);
                you_l->setText(tr("No"));
            }
        } else {
            ClearHighlight(you_l);
            you_l->setText(tr("— (no wallet)"));
        }
    };

    interfaces::ConvergenceBlockResult tip_r;
    const bool tip_got = fetch_at(tip, tip_r);
    apply_row(tip, tip_r, tip_got,
        ui->labelTipRoundError,
        ui->labelTipRoundHeight,
        ui->labelTipRoundN,
        ui->labelTipRoundCand,
        ui->labelTipRoundWinner,
        ui->labelTipRoundYou);

    if (tip < 1) {
        dash_prev();
        ui->framePrevRound->setVisible(false);
        return;
    }
    ui->framePrevRound->setVisible(true);
    interfaces::ConvergenceBlockResult prev_r;
    const bool prev_got = fetch_at(tip - 1, prev_r);
    apply_row(tip - 1, prev_r, prev_got,
        ui->labelPrevRoundError,
        ui->labelPrevRoundHeight,
        ui->labelPrevRoundN,
        ui->labelPrevRoundCand,
        ui->labelPrevRoundWinner,
        ui->labelPrevRoundYou);
}

void ConvergencePage::updateQuadraticFeeHint()
{
    if (!walletModel || !walletModel->getOptionsModel()) {
        ui->labelQuadraticFee->setText(tr("—"));
        ui->labelTotalDeduction->setText(tr("—"));
        return;
    }
    bool ok = false;
    const CAmount amt = ui->convergeAmount->value(&ok);
    if (!ok || amt <= 0) {
        ui->labelQuadraticFee->setText(tr("—"));
        ui->labelTotalDeduction->setText(tr("—"));
        return;
    }
    const BitcoinUnit unit = walletModel->getOptionsModel()->getDisplayUnit();
    ui->labelQuadraticFee->setText(tr("None (convergence only)"));
    ui->labelTotalDeduction->setText(BitcoinUnits::formatWithUnit(unit, amt));
}

void ConvergencePage::updateConvergenceNetworkSafety()
{
    // Offline / sync gating is owned by ConvergenceStartPanel; keep page banner in sync.
    // Single refresh here — refreshPhaseCPanels must not call startPanel->refresh again.
    if (m_startPanel && m_startPanel->isVisible()) m_startPanel->refresh();
    updateRecoveryWarnings();
}

void ConvergencePage::refreshPhaseCPanels()
{
    auto refresh_if_visible = [](QWidget* w, const auto& fn) {
        if (w && w->isVisible()) fn();
    };
    refresh_if_visible(m_lifecyclePanel, [this] { m_lifecyclePanel->refresh(); });
    refresh_if_visible(m_networkPanel, [this] { m_networkPanel->refresh(); });
    // startPanel already refreshed in updateConvergenceNetworkSafety when visible
    refresh_if_visible(m_overviewPanel, [this] { m_overviewPanel->refresh(); });
    refresh_if_visible(m_resProbabilityPanel, [this] { m_resProbabilityPanel->refresh(); });
    refresh_if_visible(m_historyPanel, [this] { m_historyPanel->refresh(); });
    // updateRecoveryWarnings already ran from updateConvergenceNetworkSafety in refreshVisibleUi.
}

void ConvergencePage::updateRecoveryWarnings()
{
    if (!m_warningBanner) return;

    QString msg;
    RigorWallet::WarningLevel level = RigorWallet::WarningLevel::Info;

    if (!clientModel || !walletModel) {
        m_warningBanner->hideBanner();
        return;
    }

    const int peers = clientModel->getNumConnections();
    if (peers <= 0 && !Params().IsMockableChain()) {
        msg = tr("Wallet is offline. Connect to the RIGOR network before starting Convergence.");
        level = RigorWallet::WarningLevel::Critical;
    } else if (clientModel->node().isInitialBlockDownload()) {
        msg = tr("Syncing — wait until the wallet finishes catching up.");
        level = RigorWallet::WarningLevel::Warning;
    } else if (walletModel->wallet().privateKeysDisabled()) {
        msg = tr("Wallet locked for spending — unlock to start Convergence.");
        level = RigorWallet::WarningLevel::Warning;
    } else {
        const int tip = clientModel->getNumBlocks();
        const auto rows = RigorWallet::ConvergenceWalletData::buildHistory(walletModel, tip);
        if (const auto* pending = RigorWallet::ConvergenceWalletData::pendingRow(rows)) {
            msg = tr("Convergence in progress: %1").arg(pending->txid.left(16) + QStringLiteral("…"));
            level = RigorWallet::WarningLevel::Verification;
        } else {
            interfaces::ConvergenceUiSnapshot snap;
            if (walletModel->wallet().tryGetConvergenceUiSnapshot(snap) && snap.has_convergence) {
                const QString st = QString::fromStdString(snap.status_text);
                if (st.contains(QLatin1String("Locked"), Qt::CaseInsensitive)) {
                    msg = tr("RES not ready yet: %1").arg(st);
                    level = RigorWallet::WarningLevel::Warning;
                }
            }
        }
        // Amount entry / insufficient-ORU validation lives in ConvergenceStartPanel.
    }

    if (msg.isEmpty()) {
        m_warningBanner->hideBanner();
    } else {
        m_warningBanner->showBanner(level, msg);
    }
}

namespace {
QString humanizeConvergenceStatus(const QString& raw)
{
    const QString upper = raw.trimmed().toUpper();
    if (upper.isEmpty()) return QString();
    if (upper == QLatin1String("MATURING") || upper.contains(QLatin1String("LOCK"))) {
        return QCoreApplication::translate("ConvergencePage", "Awaiting maturity");
    }
    if (upper.contains(QLatin1String("WON"))) {
        return QCoreApplication::translate("ConvergencePage", "Mint in progress");
    }
    if (upper == QLatin1String("INCLUDED")) {
        return QCoreApplication::translate("ConvergencePage", "Included in block");
    }
    if (upper == QLatin1String("PENDING") || upper == QLatin1String("BROADCASTED")) {
        return QCoreApplication::translate("ConvergencePage", "Pending confirmation");
    }
    if (upper == QLatin1String("LOST")) {
        return QCoreApplication::translate("ConvergencePage", "No RES minted");
    }
    return raw;
}
} // namespace

void ConvergencePage::updateConvergenceStatusDisplay()
{
    if (!walletModel) return;
    interfaces::ConvergenceUiSnapshot snap;
    if (!walletModel->wallet().tryGetConvergenceUiSnapshot(snap)) return;
    if (!snap.has_convergence) {
        ui->labelConvergeStatus->setText(tr("—"));
        ui->labelConvergeCompetition->setText(tr("—"));
        ui->labelConvergeMaturity->setText(tr("—"));
        ui->labelConvergeMaturityCountdown->setText(tr("—"));
        ui->labelConvergeMaturityCountdownTitle->setVisible(false);
        ui->labelConvergeMaturityCountdown->setVisible(false);
        return;
    }
    ui->labelConvergeStatus->setText(humanizeConvergenceStatus(QString::fromStdString(snap.status_text)));
    ui->labelConvergeCompetition->setText(QString::fromStdString(snap.competition_progress));
    ui->labelConvergeMaturity->setText(QString::fromStdString(snap.maturity_progress));
    const QString cd = QString::fromStdString(snap.maturity_countdown);
    const bool show_cd = !cd.isEmpty();
    ui->labelConvergeMaturityCountdownTitle->setVisible(show_cd);
    ui->labelConvergeMaturityCountdown->setVisible(show_cd);
    ui->labelConvergeMaturityCountdown->setText(show_cd ? cd : QString());
}

void ConvergencePage::updateMinConvergenceDisplay()
{
    if (!clientModel) {
        ui->labelMinConverge->setText(tr("—"));
        return;
    }
    // Always show consensus minimum in ORU (1 ORU); do not use mORU/Tharn display units here.
    const CAmount min_amt = Consensus::MIN_CONVERGENCE_AMOUNT;
    ui->labelMinConverge->setText(BitcoinUnits::formatWithUnit(BitcoinUnit::ORU, min_amt));
    updateQuadraticFeeHint();
}

void ConvergencePage::onSendConvergenceClicked()
{
    if (!walletModel || !walletModel->getOptionsModel() || !clientModel) {
        return;
    }
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        return;
    }
    bool ok = false;
    const CAmount amt = ui->convergeAmount->value(&ok);
    if (!ok || amt <= 0) {
        QMessageBox::warning(this, tr("Convergence"),
            tr("Enter a valid convergence amount. Reason: amount must be positive. Suggestion: enter a value above zero."));
        return;
    }
    const CAmount min_amt = Consensus::MIN_CONVERGENCE_AMOUNT;
    if (amt < min_amt) {
        QMessageBox::warning(this, tr("Convergence"),
            tr("Amount is below the minimum convergence (%1). Reason: consensus requires at least 1.00 ORU per convergence transaction. Suggestion: increase the amount.")
                .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), min_amt)));
        return;
    }

    const auto balances = walletModel->getCachedBalance();
    const CAmount oru_avail = balances.oru_balance + balances.oru_unconfirmed_balance;
    if (amt > oru_avail) {
        QMessageBox::warning(this, tr("Convergence"),
            tr("Insufficient ORU available. Reason: the convergence amount exceeds your spendable and pending ORU. Suggestion: lower the amount or fund the wallet."));
        return;
    }

    const CAmount large_threshold = std::max(min_amt * 10, oru_avail / 2);
    if (oru_avail > 0 && amt >= large_threshold) {
        const auto r = QMessageBox::question(this, tr("Large convergence amount"),
            tr("You are about to convergence a large amount of ORU relative to your balance. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }

    const auto r2 = QMessageBox::question(this, tr("Confirm Convergence"),
        tr("Convergence uses ORU permanently. RES is not guaranteed. This cannot be undone after broadcast.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r2 != QMessageBox::Yes) return;

    ui->pushButtonConverge->setEnabled(false);
    ConvergencePage* self_ptr = this;
    QTimer::singleShot(2700, this, [this, self_ptr]() {
        if (self_ptr && ui->pushButtonConverge) ui->pushButtonConverge->setEnabled(true);
    });

    const auto ret = walletModel->sendConvergenceOru(amt);
    if (ret.status == WalletModel::OK) {
        QMessageBox::information(this, tr("Convergence"),
            tr("Convergence submitted. Track progress and the result on this page. RES is not guaranteed."));
        ui->convergeAmount->clear();
        updateQuadraticFeeHint();
        updateConvergenceRoundPanels();
        updateConvergenceStatusDisplay();
        refreshPhaseCPanels();
        updateRecoveryWarnings();
    }
}
