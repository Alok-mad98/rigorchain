// Copyright (c) 2011-2021 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionfilterproxy.h>

#include <qt/transactiontablemodel.h>
#include <qt/transactionrecord.h>

#include <wallet/tx_semantic.h>

#include <algorithm>
#include <cstdlib>
#include <optional>

TransactionFilterProxy::TransactionFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent),
      m_search_string(),
      typeFilter(ALL_TYPES)
{
}

bool TransactionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    int status = index.data(TransactionTableModel::StatusRole).toInt();
    if (!showInactive && status == TransactionStatus::Conflicted)
        return false;

    int type = index.data(TransactionTableModel::TypeRole).toInt();
    if (!(TYPE(type) & typeFilter))
        return false;

    const int semantic = index.data(TransactionTableModel::SemanticKindRole).toInt();
    const auto kind = static_cast<wallet::TxSemanticKind>(semantic);
    const auto row_asset = static_cast<AssetType>(
        index.data(TransactionTableModel::AssetDomainRole).toInt());
    // ORU / RES are row-asset categories (the amount column), not tx-level semantic.
    // Coinbase RES mint shares a coinbase tx classified as OruCoinbase; filtering by
    // semantic alone hides those RES rows (and Unknown semantic hid both lists).
    const bool is_convergence = kind == wallet::TxSemanticKind::OruConvergence ||
                                type == TransactionRecord::SendToConverge;
    const bool is_mint = kind == wallet::TxSemanticKind::ResMint ||
                         (type == TransactionRecord::Generated && row_asset == ASSET_RES);
    switch (semanticAssetFilter) {
    case SemanticFilter_All:
        break;
    case SemanticFilter_ORU:
        if (is_convergence || row_asset != ASSET_ORU)
            return false;
        break;
    case SemanticFilter_RES:
        if (row_asset != ASSET_RES)
            return false;
        break;
    case SemanticFilter_Convergence:
        if (!is_convergence)
            return false;
        break;
    case SemanticFilter_Mint:
        if (!is_mint)
            return false;
        break;
    }

    bool involvesWatchAddress = index.data(TransactionTableModel::WatchonlyRole).toBool();
    if (involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_No)
        return false;
    if (!involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_Yes)
        return false;

    QDateTime datetime = index.data(TransactionTableModel::DateRole).toDateTime();
    if (dateFrom && datetime < *dateFrom) return false;
    if (dateTo && datetime > *dateTo) return false;

    QString address = index.data(TransactionTableModel::AddressRole).toString();
    QString label = index.data(TransactionTableModel::LabelRole).toString();
    QString txid = index.data(TransactionTableModel::TxHashRole).toString();
    if (!address.contains(m_search_string, Qt::CaseInsensitive) &&
        !  label.contains(m_search_string, Qt::CaseInsensitive) &&
        !   txid.contains(m_search_string, Qt::CaseInsensitive)) {
        return false;
    }

    qint64 amount = llabs(index.data(TransactionTableModel::AmountRole).toLongLong());
    if (amount < minAmount)
        return false;

    return true;
}

void TransactionFilterProxy::setDateRange(const std::optional<QDateTime>& from, const std::optional<QDateTime>& to)
{
    dateFrom = from;
    dateTo = to;
    invalidateFilter();
}

void TransactionFilterProxy::setSearchString(const QString &search_string)
{
    if (m_search_string == search_string) return;
    m_search_string = search_string;
    invalidateFilter();
}

void TransactionFilterProxy::setTypeFilter(quint32 modes)
{
    this->typeFilter = modes;
    invalidateFilter();
}

void TransactionFilterProxy::setSemanticAssetFilter(SemanticAssetFilter filter)
{
    semanticAssetFilter = filter;
    invalidateFilter();
}

void TransactionFilterProxy::setMinAmount(const CAmount& minimum)
{
    this->minAmount = minimum;
    invalidateFilter();
}

void TransactionFilterProxy::setWatchOnlyFilter(WatchOnlyFilter filter)
{
    this->watchOnlyFilter = filter;
    invalidateFilter();
}

void TransactionFilterProxy::setShowInactive(bool _showInactive)
{
    this->showInactive = _showInactive;
    invalidateFilter();
}
