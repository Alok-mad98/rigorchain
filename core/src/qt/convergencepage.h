// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CONVERGENCEPAGE_H
#define BITCOIN_QT_CONVERGENCEPAGE_H

#include <QWidget>
#include <memory>

class ClientModel;
class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QTimer;
class QShowEvent;
QT_END_NAMESPACE

namespace Ui {
class ConvergencePage;
}

namespace RigorWallet {
class ConvergenceEducationPanel;
class ConvergenceOverviewPanel;
class ConvergenceLifecyclePanel;
class ConvergenceStartPanel;
class ConvergenceNetworkPanel;
class ConvergenceResProbabilityPanel;
class ConvergenceHistoryPanel;
class WarningBanner;
}

/** Standalone convergence (ORU) page — see WALLET DESIGN LOCK in overviewpage.cpp */
class ConvergencePage : public QWidget
{
    Q_OBJECT

public:
    explicit ConvergencePage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~ConvergencePage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

protected:
    void showEvent(QShowEvent* event) override;

private:
    Ui::ConvergencePage* ui;
    ClientModel* clientModel{nullptr};
    WalletModel* walletModel{nullptr};
    const PlatformStyle* m_platform_style{nullptr};

    RigorWallet::ConvergenceEducationPanel* m_educationPanel{nullptr};
    RigorWallet::ConvergenceLifecyclePanel* m_lifecyclePanel{nullptr};
    RigorWallet::ConvergenceStartPanel* m_startPanel{nullptr};
    RigorWallet::ConvergenceNetworkPanel* m_networkPanel{nullptr};
    RigorWallet::ConvergenceOverviewPanel* m_overviewPanel{nullptr};
    RigorWallet::ConvergenceResProbabilityPanel* m_resProbabilityPanel{nullptr};
    RigorWallet::WarningBanner* m_warningBanner{nullptr};
    RigorWallet::ConvergenceHistoryPanel* m_historyPanel{nullptr};
    QTimer* m_refreshDebounce{nullptr};

    void applyConvergenceTheme();
    /** Coalesce tip/wallet signals; no-op when page is not visible. */
    void scheduleUiRefresh();
    void refreshVisibleUi();
    void refreshPhaseCPanels();
    void updateRecoveryWarnings();
    void updateConvergenceAvailability();
    void setMonospacedConvergenceFont(bool use_embedded_font);
    void updateContextPanels();
    void updateMarketPanel();
    void updateQuadraticFeeHint();

private Q_SLOTS:
    void updateMinConvergenceDisplay();
    void onSendConvergenceClicked();
    void updateConvergenceStatusDisplay();
    void updateConvergenceNetworkSafety();
    /** Loads ConvergenceBlockResult for tip and tip-1 (one block = one settled convergence round). */
    void updateConvergenceRoundPanels();
};

#endif // BITCOIN_QT_CONVERGENCEPAGE_H
