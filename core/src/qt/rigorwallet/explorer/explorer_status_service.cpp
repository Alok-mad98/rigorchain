// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/explorer/explorer_status_service.h>

#include <qt/clientmodel.h> // SyncType, SynchronizationState

#include <QTimer>

namespace RigorWallet {

ExplorerStatusService::ExplorerStatusService(QObject* parent)
    : QObject(parent)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(250);
    connect(m_refreshTimer, &QTimer::timeout, this, &ExplorerStatusService::doRefresh);
}

ExplorerStatusService& ExplorerStatusService::instance()
{
    static ExplorerStatusService service;
    return service;
}

void ExplorerStatusService::setClientModel(ClientModel* model)
{
    if (m_clientModel == model) return;
    if (m_clientModel) {
        disconnect(m_clientModel, nullptr, this, nullptr);
    }
    m_clientModel = model;
    if (m_clientModel) {
        connect(m_clientModel, &ClientModel::numBlocksChanged, this, [this](int, const QDateTime&, double, SyncType, SynchronizationState) {
            refresh();
        });
    }
    refresh();
}

void ExplorerStatusService::refresh()
{
    if (!m_refreshTimer) return;
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    }
}

void ExplorerStatusService::doRefresh()
{
    m_status = ExplorerClient::fetchLiveStatus(m_clientModel);
    Q_EMIT statusUpdated(m_status);
}

} // namespace RigorWallet
