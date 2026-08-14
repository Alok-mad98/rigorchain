// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_resprobabilitypanel.h>

#include <qt/clientmodel.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/rigortheme.h>

#include <consensus/rigor_consensus_lock.h>
#include <interfaces/node.h>

#include <univalue.h>

#include <QLabel>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceResProbabilityPanel::ConvergenceResProbabilityPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorResProbabilityPanel"));
    const ColorPalette p = RigorTheme::colors();
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorResProbabilityPanel {"
                      " background: %1; border-left: 3px solid %2;"
                      " border: 1px solid %3; border-radius: 6px; padding: 8px; }")
                      .arg(RigorTheme::colorHex(p.bgSurface),
                           RigorTheme::colorHex(p.convergence),
                           RigorTheme::colorHex(p.borderSubtle)));

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("RES Probability"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    m_targetLabel = new QLabel(this);
    m_windowLabel = new QLabel(this);
    m_outcomeLabel = new QLabel(this);
    m_outcomeLabel->setWordWrap(true);
    for (QLabel* l : {m_targetLabel, m_windowLabel, m_outcomeLabel}) {
        layout->addWidget(l);
    }
    m_outcomeLabel->setText(tr("Values are reported by the node. The wallet does not compute probability or predict RES outcomes."));
}

void ConvergenceResProbabilityPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    if (isVisible()) refresh();
}

void ConvergenceResProbabilityPanel::refresh()
{
    if (!isVisible()) return;
    if (!m_clientModel) {
        m_targetLabel->setText(tr("Selection target: —"));
        m_windowLabel->setText(tr("Consensus window W = %1 blocks").arg(Consensus::CONVERGENCE_DRAW_WINDOW));
        return;
    }

    QString target_display = tr("unavailable");
    try {
        UniValue chain = m_clientModel->node().executeRpc("getblockchaininfo", UniValue(UniValue::VARR), "/");
        if (chain.isObject() && chain.exists("res_probability_target")) {
            const QString raw = QString::fromStdString(chain["res_probability_target"].get_str());
            target_display = raw.left(16) + QStringLiteral("\u2026");
        }
    } catch (...) {
    }
    m_targetLabel->setText(tr("Selection target: %1").arg(target_display));
    m_windowLabel->setText(tr("Consensus window W = %1 blocks (height-based countdown only)")
                               .arg(Consensus::CONVERGENCE_DRAW_WINDOW));

    try {
        UniValue r = m_clientModel->node().executeRpc("getconvergencestatus", UniValue(UniValue::VARR), "/");
        if (r.isObject()) {
            const int tip = r["tip_height"].getInt<int>();
            const int last_res = r["last_res_height"].getInt<int>();
            m_outcomeLabel->setText(tr("Tip height %1. Last RES mint height %2. "
                                       "Per-entry W = %3 tracking is shown in the RES CONVERGENCE panel above. "
                                       "At most one RES per block; mint is not guaranteed. "
                                       "RES is not a mining reward.")
                                        .arg(tip)
                                        .arg(last_res)
                                        .arg(Consensus::CONVERGENCE_DRAW_WINDOW));
        }
    } catch (...) {
        m_outcomeLabel->setText(tr("Node status unavailable. W = %1 remains the consensus window width.")
                                    .arg(Consensus::CONVERGENCE_DRAW_WINDOW));
    }
}

} // namespace RigorWallet
