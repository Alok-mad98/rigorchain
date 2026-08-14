// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_networkpanel.h>

#include <qt/clientmodel.h>
#include <qt/rigorwallet/rigortheme.h>

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <interfaces/node.h>

#include <univalue.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QLocale>
#include <QtGlobal>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
int g_network_stats_tip = -2;
int64_t g_network_active = 0;
int64_t g_network_oru = 0;
int64_t g_network_pending = 0;
int64_t g_network_res = 0;
int g_network_height = -1;
bool g_network_stats_ok = false;

QString FormatOruAtoms(int64_t atoms)
{
    const double oru = static_cast<double>(atoms) / static_cast<double>(COIN);
    return QString::number(oru, 'f', 2) + QStringLiteral(" ORU");
}

QString FormatResAtoms(int64_t atoms)
{
    const double res = static_cast<double>(atoms) / static_cast<double>(Consensus::RES_COIN);
    return QString::number(res, 'f', 2) + QStringLiteral(" RES");
}

QString FormatCount(int64_t n)
{
    return QLocale::system().toString(static_cast<qlonglong>(n));
}
} // namespace

ConvergenceNetworkPanel::ConvergenceNetworkPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceNetwork"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorConvergenceNetwork {"
                      " background: %1; border-left: 4px solid %2;"
                      " border: 1px solid %3; border-radius: 6px; padding: 10px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface),
                           RigorTheme::colorHex(p.accent),
                           RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("CONVERGENCE NETWORK"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Network-wide Convergence activity. ORU participants commit ORU toward deterministic RES minting."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(RigorTheme::colorHex(p.textSecondary)));
    layout->addWidget(subtitle);

    m_activeLabel = new QLabel(this);
    m_oruLabel = new QLabel(this);
    m_pendingLabel = new QLabel(this);
    m_resLabel = new QLabel(this);
    m_updatedLabel = new QLabel(this);
    m_updatedLabel->setStyleSheet(QStringLiteral("color: %1;").arg(RigorTheme::colorHex(p.textMuted)));

    for (QLabel* l : {m_activeLabel, m_oruLabel, m_pendingLabel, m_resLabel, m_updatedLabel}) {
        layout->addWidget(l);
    }

    refresh();
}

void ConvergenceNetworkPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    if (isVisible()) refresh();
}

void ConvergenceNetworkPanel::refresh()
{
    if (!isVisible()) return;
    if (!m_clientModel) {
        m_activeLabel->setText(tr("Active Convergence: —"));
        m_oruLabel->setText(tr("ORU committed: —"));
        m_pendingLabel->setText(tr("Pending Convergence periods: —"));
        m_resLabel->setText(tr("Total RES minted: —"));
        m_updatedLabel->setText(tr("Last updated: —"));
        return;
    }

    const int tip = m_clientModel->getNumBlocks();
    if (g_network_stats_ok && g_network_stats_tip == tip) {
        m_activeLabel->setText(tr("Active Convergence:\n%1").arg(FormatCount(g_network_active)));
        m_oruLabel->setText(tr("ORU committed:\n%1").arg(FormatOruAtoms(g_network_oru)));
        m_pendingLabel->setText(tr("Pending Convergence periods:\n%1").arg(FormatCount(g_network_pending)));
        m_resLabel->setText(tr("Total RES minted:\n%1").arg(FormatResAtoms(g_network_res)));
        m_updatedLabel->setText(tr("Last updated:\nBlock %1").arg(FormatCount(g_network_height)));
        return;
    }

    QElapsedTimer rpc_timer;
    rpc_timer.start();
    try {
        UniValue r = m_clientModel->node().executeRpc("getconvergencestats", UniValue(UniValue::VARR), "/");
        if (!r.isObject()) {
            throw std::runtime_error("not an object");
        }
        g_network_active = r["active_convergences"].getInt<int64_t>();
        g_network_oru = r["active_oru"].getInt<int64_t>();
        g_network_pending = r["pending_windows"].getInt<int64_t>();
        g_network_res = r["total_res_minted"].getInt<int64_t>();
        g_network_height = r["best_height"].getInt<int>();
        g_network_stats_tip = tip;
        g_network_stats_ok = true;

        m_activeLabel->setText(tr("Active Convergence:\n%1").arg(FormatCount(g_network_active)));
        m_oruLabel->setText(tr("ORU committed:\n%1").arg(FormatOruAtoms(g_network_oru)));
        m_pendingLabel->setText(tr("Pending Convergence periods:\n%1").arg(FormatCount(g_network_pending)));
        m_resLabel->setText(tr("Total RES minted:\n%1").arg(FormatResAtoms(g_network_res)));
        m_updatedLabel->setText(tr("Last updated:\nBlock %1").arg(FormatCount(g_network_height)));
        qInfo().noquote() << QStringLiteral("[ui-perf] ConvergenceNetworkPanel.getconvergencestats %1ms")
                                 .arg(rpc_timer.elapsed());
    } catch (...) {
        g_network_stats_ok = false;
        m_activeLabel->setText(tr("Active Convergence: —"));
        m_oruLabel->setText(tr("ORU committed: —"));
        m_pendingLabel->setText(tr("Pending Convergence periods: —"));
        m_resLabel->setText(tr("Total RES minted: —"));
        m_updatedLabel->setText(tr("Last updated: unavailable"));
    }
}

} // namespace RigorWallet
