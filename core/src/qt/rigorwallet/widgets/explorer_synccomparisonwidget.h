// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_SYNCCOMPARISONWIDGET_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_SYNCCOMPARISONWIDGET_H

#include <QFrame>

class QLabel;

namespace RigorWallet {

/** Node vs indexed height comparison (BUILD-44). */
class ExplorerSyncComparisonWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ExplorerSyncComparisonWidget(QWidget* parent = nullptr);

public Q_SLOTS:
    void refreshFromService();

private:
    QLabel* m_nodeLabel{nullptr};
    QLabel* m_indexedLabel{nullptr};
    QLabel* m_diffLabel{nullptr};
};

} // namespace RigorWallet

#endif
