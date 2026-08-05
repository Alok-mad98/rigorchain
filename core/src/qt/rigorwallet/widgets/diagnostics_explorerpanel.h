// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_EXPLORERPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_EXPLORERPANEL_H

#include <QFrame>

class ClientModel;

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Explorer status panel (BUILD-30). */
class DiagnosticsExplorerPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsExplorerPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    StatusBadge* m_badge{nullptr};
    QLabel* m_connectionLabel{nullptr};
    QLabel* m_lagLabel{nullptr};
    QLabel* m_heightLabel{nullptr};
};

} // namespace RigorWallet

#endif
