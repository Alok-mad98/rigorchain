// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_EDUCATIONPANEL_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_CONVERGENCE_EDUCATIONPANEL_H

#include <QFrame>

class QTextBrowser;

namespace RigorWallet {

class ConvergenceEducationPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ConvergenceEducationPanel(QWidget* parent = nullptr);
};

} // namespace RigorWallet

#endif
