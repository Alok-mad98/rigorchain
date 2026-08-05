// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_RESPROBABILITYPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_RESPROBABILITYPANEL_H

#include <QFrame>

class ClientModel;
class QLabel;

namespace RigorWallet {

/** Authority-sourced RES probability display (BUILD-19) — no local calculations. */
class ConvergenceResProbabilityPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceResProbabilityPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    QLabel* m_targetLabel{nullptr};
    QLabel* m_windowLabel{nullptr};
    QLabel* m_outcomeLabel{nullptr};
};

} // namespace RigorWallet

#endif
