// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_HEALTHWIDGET_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_HEALTHWIDGET_H

#include <QFrame>

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Explorer health summary (BUILD-45). */
class ExplorerHealthWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ExplorerHealthWidget(QWidget* parent = nullptr);

public Q_SLOTS:
    void refreshFromService();

private:
    StatusBadge* m_badge{nullptr};
    QLabel* m_reachableLabel{nullptr};
    QLabel* m_latencyLabel{nullptr};
    QLabel* m_heightLabel{nullptr};
    QLabel* m_lagLabel{nullptr};
};

} // namespace RigorWallet

#endif
