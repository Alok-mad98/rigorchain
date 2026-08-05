// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_VERIFICATIONBADGE_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_VERIFICATIONBADGE_H

#include <qt/rigorwallet/explorer/explorerclient.h>
#include <qt/rigorwallet/widgets/statusbadge.h>

#include <QFrame>

class QLabel;

namespace RigorWallet {

/** Explorer verification chip (BUILD-43). */
class ExplorerVerificationBadge : public QFrame
{
    Q_OBJECT

public:
    explicit ExplorerVerificationBadge(QWidget* parent = nullptr);

    void setVerification(ExplorerTxVerification state, int indexer_lag = -1);
    void setFromLag(int indexer_lag);

private:
    StatusBadge* m_badge{nullptr};
    QLabel* m_detailLabel{nullptr};
};

} // namespace RigorWallet

#endif
