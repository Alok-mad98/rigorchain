// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/explorer/explorer_status_service.h>

#include <qt/clientmodel.h> // SyncType, SynchronizationState

#include <atomic>
#include <thread>

#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <QtGlobal>

namespace RigorWallet {

namespace {
std::atomic_bool g_explorer_fetch_inflight{false};
} // namespace

ExplorerStatusService::ExplorerStatusService(QObject* parent)
    : QObject(parent)
{
    // Periodic poll only — never sync HTTP on every block tip (was freezing the GUI).
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(false);
    m_refreshTimer->setInterval(15000);
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
    if (!m_clientModel) {
        m_refreshTimer->stop();
        return;
    }
    // Tip changes only ensure the periodic timer is running; do not HTTP on each tip.
    connect(m_clientModel, &ClientModel::numBlocksChanged, this, [this](int, const QDateTime&, double, SyncType, SynchronizationState) {
        refresh();
    });
    if (ExplorerClient::isConfigured()) {
        if (!m_refreshTimer->isActive()) m_refreshTimer->start();
        // Defer first fetch so wallet UI can paint before explorer work starts.
        QTimer::singleShot(2500, this, &ExplorerStatusService::doRefresh);
    } else {
        m_refreshTimer->stop();
        m_status = ExplorerLiveStatus{};
        Q_EMIT statusUpdated(m_status);
    }
}

void ExplorerStatusService::refresh()
{
    if (!ExplorerClient::isConfigured()) return;
    // Coalesce tip storms into the next periodic tick (or start timer if idle).
    if (!m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    }
}

void ExplorerStatusService::applyFetchResult(const ExplorerLiveStatus& status, qint64 elapsed_ms)
{
    m_status = status;
    g_explorer_fetch_inflight.store(false);
    qInfo().noquote() << QStringLiteral("[ui-perf] ExplorerStatusService.fetchLiveStatus %1ms (worker)")
                             .arg(elapsed_ms);
    Q_EMIT statusUpdated(m_status);
}

void ExplorerStatusService::doRefresh()
{
    if (!ExplorerClient::isConfigured()) {
        m_status = ExplorerLiveStatus{};
        m_status.configured = false;
        Q_EMIT statusUpdated(m_status);
        return;
    }
    // fetchLiveStatus uses a local QEventLoop (up to ~5s × 2). Never run that on the GUI thread.
    bool expected = false;
    if (!g_explorer_fetch_inflight.compare_exchange_strong(expected, true)) {
        return;
    }

    const int node_height = m_clientModel ? m_clientModel->getNumBlocks() : -1;
    std::thread([this, node_height]() {
        QElapsedTimer t;
        t.start();
        ExplorerLiveStatus status = ExplorerClient::fetchLiveStatus(nullptr);
        const qint64 ms = t.elapsed();
        if (node_height >= 0) status.node_height = node_height;
        if (status.node_height >= 0 && status.indexed_height >= 0) {
            status.indexer_lag = qMax(0, status.node_height - status.indexed_height);
        }
        // Functor queued invoke (no QMetaType registration required for ExplorerLiveStatus).
        QMetaObject::invokeMethod(this, [this, status, ms]() {
            applyFetchResult(status, ms);
        }, Qt::QueuedConnection);
    }).detach();
}

} // namespace RigorWallet
