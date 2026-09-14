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
class ConvergenceNetworkPanel;
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
    RigorWallet::ConvergenceNetworkPanel* m_networkPanel{nullptr};
    RigorWallet::WarningBanner* m_warningBanner{nullptr};
    QTimer* m_refreshDebounce{nullptr};

    bool m_mint_preview_ok{false};
    bool m_mint_authorized{false};
    int m_mint_preview_tip{-2};
    /** Confirming height h for R · (2^25+h)^2 ≤ B << 40. Refreshed every tip. */
    int m_mint_destroy_cost_height{0};

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
    void loadMintPreviewParams();
    void updateMintedResPreview();

private Q_SLOTS:
    void updateMinConvergenceDisplay();
    void onSendConvergenceClicked();
    void updateConvergenceNetworkSafety();
    /** Loads ConvergenceBlockResult for tip and tip-1. */
    void updateConvergenceRoundPanels();
};

#endif // BITCOIN_QT_CONVERGENCEPAGE_H
