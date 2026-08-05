// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_VERIFICATIONPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_VERIFICATIONPANEL_H

#include <QFrame>

class ClientModel;

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Verification status panel (BUILD-32). */
class DiagnosticsVerificationPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsVerificationPanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void refresh();

private:
    ClientModel* m_clientModel{nullptr};
    StatusBadge* m_badge{nullptr};
    QLabel* m_proofLabel{nullptr};
    QLabel* m_archiveLabel{nullptr};
    QLabel* m_simulationLabel{nullptr};
    QLabel* m_equivLabel{nullptr};
};

} // namespace RigorWallet

#endif
