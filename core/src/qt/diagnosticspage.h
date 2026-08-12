// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DIAGNOSTICSPAGE_H
#define BITCOIN_QT_DIAGNOSTICSPAGE_H

#include <QWidget>

class ClientModel;
class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QTimer;
class QShowEvent;
QT_END_NAMESPACE

namespace RigorWallet {
class WarningBanner;
class DiagnosticsNodePanel;
class DiagnosticsWalletPanel;
class DiagnosticsExplorerPanel;
class DiagnosticsConvergencePanel;
class DiagnosticsVerificationPanel;
class ExplorerHealthWidget;
class ExplorerSyncComparisonWidget;
class ExplorerObserverHooks;
class ReleaseGatePanel;
}

/** Dedicated diagnostics visibility page (BUILD-27). */
class DiagnosticsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticsPage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void scheduleRefresh();
    void refreshAll();
    void updateWarnings();

    ClientModel* clientModel{nullptr};
    WalletModel* walletModel{nullptr};
    const PlatformStyle* m_platform_style{nullptr};
    QTimer* m_refreshDebounce{nullptr};

    RigorWallet::WarningBanner* m_warningBanner{nullptr};
    RigorWallet::DiagnosticsNodePanel* m_nodePanel{nullptr};
    RigorWallet::DiagnosticsWalletPanel* m_walletPanel{nullptr};
    RigorWallet::DiagnosticsExplorerPanel* m_explorerPanel{nullptr};
    RigorWallet::DiagnosticsConvergencePanel* m_convergencePanel{nullptr};
    RigorWallet::DiagnosticsVerificationPanel* m_verificationPanel{nullptr};
    RigorWallet::ReleaseGatePanel* m_releaseGatePanel{nullptr};
    RigorWallet::ExplorerHealthWidget* m_explorerHealth{nullptr};
    RigorWallet::ExplorerSyncComparisonWidget* m_explorerSync{nullptr};
    RigorWallet::ExplorerObserverHooks* m_explorerObserverHooks{nullptr};
};

#endif // BITCOIN_QT_DIAGNOSTICSPAGE_H
