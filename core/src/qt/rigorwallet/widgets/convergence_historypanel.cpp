// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_historypanel.h>

#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/emptystatewidget.h>
#include <qt/walletmodel.h>

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <wallet/convergence_window_ui.h>

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString UserStatusLabel(const ConvergenceHistoryRow& row)
{
    switch (row.lifecycle) {
    case wallet::ConvergenceUiLifecycle::CompletedAllocated:
    case wallet::ConvergenceUiLifecycle::CompletedNoAllocation:
        return QObject::tr("Completed");
    case wallet::ConvergenceUiLifecycle::Submitted:
    case wallet::ConvergenceUiLifecycle::ActiveWindow:
    case wallet::ConvergenceUiLifecycle::FinalizationPending:
        return QObject::tr("In progress");
    }
    return QObject::tr("In progress");
}

QString UserResultLabel(const ConvergenceHistoryRow& row)
{
    switch (row.lifecycle) {
    case wallet::ConvergenceUiLifecycle::CompletedAllocated: {
        const CAmount atoms = row.res_amount > 0 ? row.res_amount : Consensus::RES_REWARD_AMOUNT;
        return QString::number(static_cast<double>(atoms) / Consensus::RES_COIN, 'f', 2) +
               QStringLiteral(" RES minted");
    }
    case wallet::ConvergenceUiLifecycle::CompletedNoAllocation:
        return QObject::tr("No RES minted");
    case wallet::ConvergenceUiLifecycle::Submitted:
    case wallet::ConvergenceUiLifecycle::ActiveWindow:
    case wallet::ConvergenceUiLifecycle::FinalizationPending:
        return QObject::tr("Waiting for confirmation");
    }
    return QObject::tr("Waiting for confirmation");
}
} // namespace

ConvergenceHistoryPanel::ConvergenceHistoryPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceHistory"));
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("History"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    m_stack = new QStackedWidget(this);
    m_empty = new EmptyStateWidget(m_stack);
    m_empty->setHeading(tr("No Convergence history yet"));
    m_empty->setBody(tr("When you start a Convergence, it will appear here with status and result."));

    auto* tablePage = new QWidget(m_stack);
    auto* tableLayout = new QVBoxLayout(tablePage);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    // User columns: Date, ORU, Status, Result (block/digest/candidate details → Diagnostics)
    m_table = new QTableWidget(0, 4, tablePage);
    m_table->setHorizontalHeaderLabels({
        tr("Date"),
        tr("ORU"),
        tr("Status"),
        tr("Result"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    tableLayout->addWidget(m_table);

    m_stack->addWidget(m_empty);
    m_stack->addWidget(tablePage);
    layout->addWidget(m_stack);
}

void ConvergenceHistoryPanel::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    refresh();
}

void ConvergenceHistoryPanel::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    refresh();
}

void ConvergenceHistoryPanel::refresh()
{
    m_table->setRowCount(0);
    if (!m_walletModel) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    const int tip = m_clientModel ? m_clientModel->getNumBlocks() : -1;
    const auto rows = ConvergenceWalletData::buildHistory(m_walletModel, tip);
    if (rows.empty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_stack->setCurrentIndex(1);
    setUpdatesEnabled(false);
    m_table->setRowCount(static_cast<int>(rows.size()));
    int i = 0;
    for (const ConvergenceHistoryRow& row : rows) {
        m_table->setItem(i, 0, new QTableWidgetItem(GUIUtil::dateTimeStr(QDateTime::fromSecsSinceEpoch(row.timestamp))));

        QString oru = tr("—");
        if (row.oru_amount > 0) {
            oru = QString::number(static_cast<double>(row.oru_amount) / COIN, 'f', 2) + QStringLiteral(" ORU");
        }
        auto* oru_item = new QTableWidgetItem(oru);
        if (!row.txid.isEmpty()) {
            oru_item->setToolTip(row.txid);
        }
        m_table->setItem(i, 1, oru_item);

        m_table->setItem(i, 2, new QTableWidgetItem(UserStatusLabel(row)));
        m_table->setItem(i, 3, new QTableWidgetItem(UserResultLabel(row)));
        ++i;
    }
    setUpdatesEnabled(true);
}

} // namespace RigorWallet
