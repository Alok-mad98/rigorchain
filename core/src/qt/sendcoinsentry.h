// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SENDCOINSENTRY_H
#define BITCOIN_QT_SENDCOINSENTRY_H

#include <qt/sendcoinsrecipient.h>

#include <primitives/transaction.h>

#include <QWidget>

class WalletModel;
class PlatformStyle;

namespace interfaces {
class Node;
} // namespace interfaces

namespace Ui {
    class SendCoinsEntry;
}

/**
 * A single entry in the dialog for sending bitcoins.
 */
class SendCoinsEntry : public QWidget
{
    Q_OBJECT

public:
    explicit SendCoinsEntry(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~SendCoinsEntry();

    void setModel(WalletModel *model);
    bool validate(interfaces::Node& node);
    SendCoinsRecipient getValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();

    void setValue(const SendCoinsRecipient &value);
    void setAddress(const QString &address);
    void setAmount(const CAmount &amount);

    void setSendAsset(AssetType asset);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases
     *  (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void setFocus();

public Q_SLOTS:
    void clear();
    void checkSubtractFeeFromAmount();

Q_SIGNALS:
    void removeEntry(SendCoinsEntry *entry);
    void useAvailableBalance(SendCoinsEntry* entry);
    void payAmountChanged();
    void subtractFeeFromAmountChanged();
    /** Clipboard or hand-off contained a full rigor: URI (with asset=); parent should sync send asset. */
    void uriPasted(const SendCoinsRecipient& recipient);
    /** Recipient address implies native asset domain (RFC-002 §6.3). */
    void payAddressAssetDetected(AssetType asset);

private Q_SLOTS:
    void deleteClicked();
    void useAvailableBalanceClicked();
    void on_payTo_textChanged(const QString &address);
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void updateDisplayUnit();

protected:
    void changeEvent(QEvent* e) override;

private:
    SendCoinsRecipient recipient;
    Ui::SendCoinsEntry *ui;
    WalletModel* model{nullptr};
    const PlatformStyle *platformStyle;

    bool updateLabel(const QString &address);
};

#endif // BITCOIN_QT_SENDCOINSENTRY_H
