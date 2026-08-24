// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletview.h>

#include <qt/rigorwallet/explorer/explorer_status_service.h>
#include <qt/rigorwallet/widgets/firstrunwizard.h>

#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/convergencepage.h>
#include <qt/overviewpage.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/sendcoinsdialog.h>
#include <qt/keymanagementdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <node/interface_ui.h>
#include <util/strencodings.h>

#include <QAction>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>
#include <QtGlobal>
#include <QVBoxLayout>

WalletView::WalletView(WalletModel* wallet_model, const PlatformStyle* _platformStyle, QWidget* parent)
    : QStackedWidget(parent),
      walletModel(wallet_model),
      platformStyle(_platformStyle)
{
    assert(walletModel);

    // Overview first so Load Wallets can return to a responsive main window quickly.
    // Heavier tabs (history / send / convergence) are built on idle timers.
    overviewPage = new OverviewPage(platformStyle);
    overviewPage->setWalletModel(walletModel);
    addWidget(overviewPage);
    setCurrentWidget(overviewPage);

    connect(overviewPage, &OverviewPage::transactionClicked, this, &WalletView::transactionClicked);
    connect(overviewPage, &OverviewPage::outOfSyncWarningClicked, this, &WalletView::outOfSyncWarningClicked);
    connect(this, &WalletView::setPrivacy, overviewPage, &OverviewPage::setPrivacy);

    connect(walletModel, &WalletModel::message, this, &WalletView::message);
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &WalletView::encryptionStatusChanged);
    connect(walletModel->getTransactionTableModel(), &TransactionTableModel::rowsInserted, this, &WalletView::processNewTransaction);
    connect(walletModel, &WalletModel::requireUnlock, this, &WalletView::unlockWallet);
    connect(walletModel, &WalletModel::showProgress, this, &WalletView::showProgress);

    QTimer::singleShot(0, this, &WalletView::buildDeferredPages);
    qInfo().noquote() << QStringLiteral("[ui-perf] WalletView constructed (overview attached; deferred pages pending)");
}

WalletView::~WalletView() = default;

void WalletView::buildDeferredPages()
{
    if (m_deferred_pages_ready || m_deferred_build_started) return;
    m_deferred_build_started = true;

    // One page group per event-loop turn so the UI can paint and Windows does not
    // mark the process as Not Responding during first wallet attach.
    QElapsedTimer deferred;
    deferred.start();
    QTimer::singleShot(0, this, [this, deferred]() mutable {
        ensureHistoryPage();
        QTimer::singleShot(0, this, [this, deferred]() mutable {
            ensureReceiveSendPages();
            QTimer::singleShot(0, this, [this, deferred]() mutable {
                ensureConvergencePage();
                m_deferred_pages_ready = true;
                if (clientModel) {
                    applyClientModelToPages();
                }
                qInfo().noquote() << QStringLiteral("[ui-perf] WalletView deferred pages ready %1ms")
                                         .arg(deferred.elapsed());
            });
        });
    });
}

void WalletView::ensureHistoryPage()
{
    if (transactionsPage) return;

    transactionsPage = new QWidget(this);
    auto* vbox = new QVBoxLayout();
    auto* hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    transactionView->setModel(walletModel);

    vbox->addWidget(transactionView);
    auto* exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);
    addWidget(transactionsPage);

    connect(overviewPage, &OverviewPage::transactionClicked, transactionView, qOverload<const QModelIndex&>(&TransactionView::focusTransaction));
    connect(exportButton, &QPushButton::clicked, transactionView, &TransactionView::exportClicked);
    connect(transactionView, &TransactionView::message, this, &WalletView::message);
    connect(this, &WalletView::setPrivacy, this, &WalletView::disableTransactionView);
}

void WalletView::ensureReceiveSendPages()
{
    if (receiveCoinsPage && sendCoinsPage) return;

    if (!receiveCoinsPage) {
        receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
        receiveCoinsPage->setModel(walletModel);
        addWidget(receiveCoinsPage);
    }
    if (!sendCoinsPage) {
        sendCoinsPage = new SendCoinsDialog(platformStyle);
        sendCoinsPage->setModel(walletModel);
        addWidget(sendCoinsPage);
        connect(sendCoinsPage, &SendCoinsDialog::coinsSent, this, &WalletView::coinsSent);
        if (transactionView) {
            connect(sendCoinsPage, &SendCoinsDialog::coinsSent, transactionView, qOverload<const uint256&>(&TransactionView::focusTransaction));
        }
        connect(sendCoinsPage, &SendCoinsDialog::message, this, &WalletView::message);
        if (clientModel) {
            sendCoinsPage->setClientModel(clientModel);
        }
    }
    if (!usedSendingAddressesPage) {
        usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
        usedSendingAddressesPage->setModel(walletModel->getAddressTableModel());
    }
    if (!usedReceivingAddressesPage) {
        usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
        usedReceivingAddressesPage->setModel(walletModel->getAddressTableModel());
    }
}

void WalletView::ensureConvergencePage()
{
    if (convergencePage) return;
    convergencePage = new ConvergencePage(platformStyle);
    convergencePage->setWalletModel(walletModel);
    addWidget(convergencePage);
    if (clientModel) {
        convergencePage->setClientModel(clientModel);
    }
}

void WalletView::ensureAllPages()
{
    ensureHistoryPage();
    ensureReceiveSendPages();
    ensureConvergencePage();
    m_deferred_pages_ready = true;
}

void WalletView::applyClientModelToPages()
{
    if (!clientModel) return;
    if (overviewPage) overviewPage->setClientModel(clientModel);
    if (convergencePage) convergencePage->setClientModel(clientModel);
    if (sendCoinsPage) sendCoinsPage->setClientModel(clientModel);
    RigorWallet::ExplorerStatusService::instance().setClientModel(clientModel);
    if (walletModel) walletModel->setClientModel(clientModel);
}

void WalletView::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    applyClientModelToPages();
    maybeShowFirstRunWizard();
}

void WalletView::maybeShowFirstRunWizard()
{
    if (!clientModel || !walletModel) return;
    if (!RigorWallet::FirstRunWizard::shouldShow()) return;
    RigorWallet::FirstRunWizard wizard(clientModel, walletModel, this);
    wizard.exec();
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!clientModel || clientModel->node().isInitialBlockDownload()) {
        return;
    }

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = GUIUtil::HtmlEscape(ttm->data(index, TransactionTableModel::LabelRole).toString());

    const QModelIndex row_index = ttm->index(start, 0, parent);
    const AssetType amount_asset = static_cast<AssetType>(
        ttm->data(row_index, TransactionTableModel::AssetDomainRole).toInt());
    Q_EMIT incomingTransaction(date, amount_asset, amount, type, address, label, GUIUtil::HtmlEscape(walletModel->getWalletName()));
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    QElapsedTimer t;
    t.start();
    ensureHistoryPage();
    setCurrentWidget(transactionsPage);
    qInfo().noquote() << QStringLiteral("[ui-perf] WalletView.gotoHistoryPage %1ms").arg(t.elapsed());
}

void WalletView::gotoConvergencePage()
{
    QElapsedTimer t;
    t.start();
    ensureConvergencePage();
    setCurrentWidget(convergencePage);
    qInfo().noquote() << QStringLiteral("[ui-perf] WalletView.gotoConvergencePage %1ms").arg(t.elapsed());
}

void WalletView::gotoReceiveCoinsPage()
{
    ensureReceiveSendPages();
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    ensureReceiveSendPages();
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void WalletView::gotoKeyManagement()
{
    auto* dlg = new KeyManagementDialog(platformStyle, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(walletModel);
    GUIUtil::ShowModalDialogAsynchronously(dlg);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    ensureReceiveSendPages();
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::encryptWallet()
{
    auto dlg = new AskPassphraseDialog(AskPassphraseDialog::Encrypt, this);
    dlg->setModel(walletModel);
    connect(dlg, &QDialog::finished, this, &WalletView::encryptionStatusChanged);
    GUIUtil::ShowModalDialogAsynchronously(dlg);
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        //: Name of the wallet data file format.
        tr("Wallet Data") + QLatin1String(" (*.dat)"), nullptr);

    if (filename.isEmpty())
        return;

    if (!walletModel->wallet().backupWallet(filename.toLocal8Bit().data())) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
        }
    else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    auto dlg = new AskPassphraseDialog(AskPassphraseDialog::ChangePass, this);
    dlg->setModel(walletModel);
    GUIUtil::ShowModalDialogAsynchronously(dlg);
}

void WalletView::unlockWallet()
{
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        // A modal dialog must be synchronous here as expected
        // in the WalletModel::requestUnlock() function.
        dlg.exec();
    }
}

void WalletView::usedSendingAddresses()
{
    ensureReceiveSendPages();
    GUIUtil::bringToFront(usedSendingAddressesPage);
}

void WalletView::usedReceivingAddresses()
{
    ensureReceiveSendPages();
    GUIUtil::bringToFront(usedReceivingAddressesPage);
}

void WalletView::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, tr("Cancel"), 0, 100);
        GUIUtil::PolishProgressDialog(progressDialog);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
            progressDialog = nullptr;
        }
    } else if (progressDialog) {
        if (progressDialog->wasCanceled()) {
            getWalletModel()->wallet().abortRescan();
        } else {
            progressDialog->setValue(nProgress);
        }
    }
}

void WalletView::disableTransactionView(bool disable)
{
    if (transactionView) {
        transactionView->setDisabled(disable);
    }
}
