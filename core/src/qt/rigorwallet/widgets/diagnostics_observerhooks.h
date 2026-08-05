// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_OBSERVERHOOKS_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_DIAGNOSTICS_OBSERVERHOOKS_H

#include <QFrame>

class QPushButton;

namespace RigorWallet {

/** Future observer integration placeholders (BUILD-37). */
class DiagnosticsObserverHooks : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsObserverHooks(QWidget* parent = nullptr);

private:
    QPushButton* m_openObserver{nullptr};
    QPushButton* m_exportObserver{nullptr};
};

} // namespace RigorWallet

#endif
