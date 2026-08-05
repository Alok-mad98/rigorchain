// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_historypanel.h>

#include <qt/guiutil.h>
#include <qt/rigorwallet/assetcontext.h>
#include <qt/rigorwallet/convergence/convergence_wallet_data.h>
#include <qt/rigorwallet/rigortheme.h>
#include <qt/rigorwallet/widgets/emptystatewidget.h>
#include <qt/walletmodel.h>

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace RigorWallet {

namespace {
QString TruncateMiddleHex(const QString& hex)
{
    if (hex.size() <= 25) return hex;
    return hex.left(12) + QStringLiteral("\u2026") + hex.right(12);
}
} // namespace

ConvergenceHistoryPanel::ConvergenceHistoryPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceHistory"));
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Convergence History %1").arg(AssetContext::authorityLabel(DataAuthority::Wallet)), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    m_stack = new QStackedWidget(this);
    m_empty = new EmptyStateWidget(m_stack);
    m_empty->setHeading(tr("No convergence history yet"));
    m_empty->setBody(tr("When you submit a convergence transaction, it will appear here with status and any linked RES mint."));

    auto* tablePage = new QWidget(m_stack);
    auto* tableLayout = new QVBoxLayout(tablePage);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(0, 5, tablePage);
    m_table->setHorizontalHeaderLabels({
        tr("Txid"),
        tr("Height"),
        tr("Status"),
        tr("Linked mint"),
        tr("Time"),
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

    const auto rows = ConvergenceWalletData::buildHistory(m_walletModel);
    if (rows.empty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_stack->setCurrentIndex(1);
    setUpdatesEnabled(false);
    m_table->setRowCount(static_cast<int>(rows.size()));
    int i = 0;
    for (const ConvergenceHistoryRow& row : rows) {
        auto* tx_item = new QTableWidgetItem(TruncateMiddleHex(row.txid));
        tx_item->setToolTip(row.txid);
        m_table->setItem(i, 0, tx_item);
        m_table->setItem(i, 1, new QTableWidgetItem(row.block_height >= 0 ? QString::number(row.block_height) : tr("—")));
        m_table->setItem(i, 2, new QTableWidgetItem(row.status_label));
        const QString mint = row.linked_mint_txid.isEmpty() ? tr("—") : TruncateMiddleHex(row.linked_mint_txid);
        auto* mint_item = new QTableWidgetItem(mint);
        if (!row.linked_mint_txid.isEmpty()) {
            mint_item->setToolTip(tr("Convergence → Mint\n%1 → %2").arg(row.txid, row.linked_mint_txid));
        }
        m_table->setItem(i, 3, mint_item);
        m_table->setItem(i, 4, new QTableWidgetItem(GUIUtil::dateTimeStr(QDateTime::fromSecsSinceEpoch(row.timestamp))));
        ++i;
    }
    setUpdatesEnabled(true);
}

} // namespace RigorWallet
