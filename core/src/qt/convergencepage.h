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

namespace Ui {
class ConvergencePage;
}

namespace RigorWallet {
class ConvergenceEducationPanel;
class ConvergenceOverviewPanel;
class ConvergenceResProbabilityPanel;
class ConvergenceHistoryPanel;
class ConvergenceExplorerHooks;
class WarningBanner;
}

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

/** Standalone convergence (ORU) page — see WALLET DESIGN LOCK in overviewpage.cpp */
class ConvergencePage : public QWidget
{
    Q_OBJECT

public:
    explicit ConvergencePage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~ConvergencePage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

private:
    Ui::ConvergencePage* ui;
    ClientModel* clientModel{nullptr};
    WalletModel* walletModel{nullptr};
    const PlatformStyle* m_platform_style{nullptr};

    QLabel* m_convergenceSafetyLabel{nullptr};

    RigorWallet::ConvergenceEducationPanel* m_educationPanel{nullptr};
    RigorWallet::ConvergenceOverviewPanel* m_overviewPanel{nullptr};
    RigorWallet::ConvergenceResProbabilityPanel* m_resProbabilityPanel{nullptr};
    RigorWallet::WarningBanner* m_warningBanner{nullptr};
    RigorWallet::ConvergenceHistoryPanel* m_historyPanel{nullptr};
    RigorWallet::ConvergenceExplorerHooks* m_explorerHooks{nullptr};

    void applyConvergenceTheme();
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
