// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_NODEPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_NODEPANEL_H

#include <QFrame>

class ClientModel;

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Node status panel (BUILD-28). */
class DiagnosticsNodePanel : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsNodePanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    StatusBadge* m_badge{nullptr};
    QLabel* m_heightLabel{nullptr};
    QLabel* m_headersLabel{nullptr};
    QLabel* m_peersLabel{nullptr};
    QLabel* m_syncLabel{nullptr};
    QLabel* m_hashLabel{nullptr};
};

} // namespace RigorWallet

#endif
