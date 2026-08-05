// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_EXPLORERHOOKS_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_EXPLORERHOOKS_H

#include <QFrame>
#include <QString>

class QPushButton;

namespace RigorWallet {

class ExplorerVerificationBadge;

/** Convergence and mint explorer links (BUILD-41 / BUILD-42). */
class ConvergenceExplorerHooks : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceExplorerHooks(QWidget* parent = nullptr);

    QPushButton* viewConvergenceButton() const { return m_viewConvergence; }
    QPushButton* viewMintButton() const { return m_viewMint; }

    void setTargetTxids(const QString& convergence_txid, const QString& mint_txid);
    void refreshVerification();

private:
    QPushButton* m_viewConvergence{nullptr};
    QPushButton* m_viewMint{nullptr};
    ExplorerVerificationBadge* m_verificationBadge{nullptr};
    QString m_convergenceTxid;
    QString m_mintTxid;
};

} // namespace RigorWallet

#endif
