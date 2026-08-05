// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_WALLETPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_WALLETPANEL_H

#include <QFrame>

class WalletModel;

class QLabel;

namespace RigorWallet {

class StatusBadge;

/** Wallet status panel (BUILD-29). */
class DiagnosticsWalletPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsWalletPanel(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);
    void refresh();

private:
    WalletModel* m_walletModel{nullptr};
    StatusBadge* m_badge{nullptr};
    QLabel* m_loadedLabel{nullptr};
    QLabel* m_encryptedLabel{nullptr};
    QLabel* m_unlockedLabel{nullptr};
    QLabel* m_backupLabel{nullptr};
};

} // namespace RigorWallet

#endif
