// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_RELEASE_GATE_PANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_RELEASE_GATE_PANEL_H

#include <QFrame>

class ClientModel;
class QLabel;
class QProgressBar;
class QVBoxLayout;
class WalletModel;

namespace RigorWallet {

/** Release gate status display — read-only. */
class ReleaseGatePanel : public QFrame
{
    Q_OBJECT

public:
    explicit ReleaseGatePanel(QWidget* parent = nullptr);

    void setClientModel(ClientModel* model);
    void setWalletModel(WalletModel* model);
    void refresh();

private:
    void exportSnapshots();

    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_gateLabel{nullptr};
    QLabel* m_testnetLabel{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_blockersLabel{nullptr};
    QLabel* m_nextActionLabel{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_RELEASE_GATE_PANEL_H
