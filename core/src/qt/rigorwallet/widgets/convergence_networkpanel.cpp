// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_networkpanel.h>

#include <qt/clientmodel.h>
#include <qt/rigorwallet/rigortheme.h>

#include <consensus/amount.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <util/moneystr.h>
#include <interfaces/node.h>

#include <univalue.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QLabel>
#include <QLocale>
#include <QSizePolicy>
#include <QString>
#include <QtGlobal>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
int g_network_stats_tip = -2;
QString g_network_res_text;
int64_t g_network_rate = 0;
int64_t g_network_next_rate = 0;
int g_network_next_height = -1;
int g_network_height = -1;
bool g_network_authorized = false;
bool g_network_stats_ok = false;

QString FormatCount(int64_t n)
{
    return QLocale::system().toString(static_cast<qlonglong>(n));
}

QString FormatOruPerRes(CAmount rate)
{
    return QString::fromStdString(FormatMoney(rate)) + QStringLiteral(" ORU per RES");
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

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    auto* title = new QLabel(tr("CONVERGENCE NETWORK"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        tr("Destroy ORU to mint RES at the current protocol cost, not a market value."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(RigorTheme::colorHex(p.textSecondary)));
    layout->addWidget(subtitle);

    m_priceLabel = new QLabel(this);
    m_priceLabel->setObjectName(QStringLiteral("labelCurrentPrice"));
    m_nextPriceLabel = new QLabel(this);
    m_nextPriceLabel->setObjectName(QStringLiteral("labelNextEpochCost"));
    m_resLabel = new QLabel(this);
    m_resLabel->setObjectName(QStringLiteral("labelResMinted"));
    m_updatedLabel = new QLabel(this);
    m_updatedLabel->setObjectName(QStringLiteral("labelLastUpdated"));
    m_updatedLabel->setStyleSheet(QStringLiteral("color: %1;").arg(RigorTheme::colorHex(p.textMuted)));

    for (QLabel* l : {title, subtitle, m_priceLabel, m_nextPriceLabel, m_resLabel, m_updatedLabel}) {
        l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        if (l != title && l != subtitle) layout->addWidget(l);
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
    auto apply = [this]() {
        if (!g_network_authorized) {
            m_priceLabel->setText(tr("Unable to read the confirming-height mint cost."));
            m_nextPriceLabel->setText(QString());
        } else {
            m_priceLabel->setText(tr("Current cost: %1").arg(FormatOruPerRes(g_network_rate)));
            if (g_network_next_rate > 0 && g_network_next_height >= 0) {
                m_nextPriceLabel->setText(
                    tr("From block %1 the cost becomes %2.")
                        .arg(FormatCount(g_network_next_height))
                        .arg(FormatOruPerRes(g_network_next_rate)));
            } else {
                m_nextPriceLabel->setText(tr("Next-block cost: —"));
            }
        }
        m_resLabel->setText(tr("Total RES minted:\n%1").arg(g_network_res_text));
        m_updatedLabel->setText(tr("Last updated:\nBlock %1").arg(FormatCount(g_network_height)));
    };

    if (!m_clientModel) {
        m_priceLabel->setText(tr("Current cost: —"));
        m_nextPriceLabel->setText(tr("Next-block cost: —"));
        m_resLabel->setText(tr("Total RES minted: —"));
        m_updatedLabel->setText(tr("Last updated: —"));
        return;
    }

    const int tip = m_clientModel->getNumBlocks();
    if (g_network_stats_ok && g_network_stats_tip == tip) {
        apply();
        return;
    }

    QElapsedTimer rpc_timer;
    rpc_timer.start();
    try {
        UniValue r = m_clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
        if (!r.isObject()) {
            throw std::runtime_error("not an object");
        }
        g_network_authorized = r["issuance_authorized"].get_bool();
        g_network_rate = g_network_authorized && r.exists("destroy_cost") ? r["destroy_cost"].getInt<int64_t>() : 0;
        g_network_next_rate = r.exists("destroy_cost_next") ? r["destroy_cost_next"].getInt<int64_t>() : 0;
        // RFC-020: next_height is confirm_height + 1.
        g_network_next_height = r.exists("next_height") ? r["next_height"].getInt<int>() : -1;
        g_network_height = r["tip_height"].getInt<int>();
        g_network_res_text = QString::fromStdString(r["res_total"].getValStr());
        g_network_stats_tip = tip;
        g_network_stats_ok = true;
        apply();
        qInfo().noquote() << QStringLiteral("[ui-perf] ConvergenceNetworkPanel.getconvergencestatus %1ms")
                                 .arg(rpc_timer.elapsed());
    } catch (...) {
        g_network_stats_ok = false;
        m_priceLabel->setText(tr("Current cost: —"));
        m_nextPriceLabel->setText(tr("Next-block cost: —"));
        m_resLabel->setText(tr("Total RES minted: —"));
        m_updatedLabel->setText(tr("Last updated: unavailable"));
    }
}

} // namespace RigorWallet
