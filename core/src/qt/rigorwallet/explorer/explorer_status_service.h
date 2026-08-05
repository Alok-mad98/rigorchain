// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORER_STATUS_SERVICE_H
#define BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORER_STATUS_SERVICE_H

#include <qt/rigorwallet/explorer/explorerclient.h>

#include <QObject>

class ClientModel;
class QTimer;

namespace RigorWallet {

/** Tracks explorer connection and lag (BUILD-39). */
class ExplorerStatusService : public QObject
{
    Q_OBJECT

public:
    static ExplorerStatusService& instance();

    void setClientModel(ClientModel* model);
    ExplorerLiveStatus status() const { return m_status; }

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void doRefresh();

Q_SIGNALS:
    void statusUpdated(const RigorWallet::ExplorerLiveStatus& status);

private:
    explicit ExplorerStatusService(QObject* parent = nullptr);

    ClientModel* m_clientModel{nullptr};
    ExplorerLiveStatus m_status;
    QTimer* m_refreshTimer{nullptr};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_EXPLORER_EXPLORER_STATUS_SERVICE_H
