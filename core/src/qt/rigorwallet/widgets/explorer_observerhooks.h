// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_OBSERVERHOOKS_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_EXPLORER_OBSERVERHOOKS_H

#include <QFrame>

class QPushButton;

namespace RigorWallet {

/** Future observer deep-link placeholders (BUILD-48). */
class ExplorerObserverHooks : public QFrame
{
    Q_OBJECT

public:
    explicit ExplorerObserverHooks(QWidget* parent = nullptr);

private:
    QPushButton* m_observerTx{nullptr};
    QPushButton* m_observerMint{nullptr};
};

} // namespace RigorWallet

#endif
