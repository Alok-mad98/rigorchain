// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/explorer_observerhooks.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace RigorWallet {

ExplorerObserverHooks::ExplorerObserverHooks(QWidget* parent)
    : QFrame(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->addWidget(new QLabel(tr("Observer (future):"), this));
    m_observerTx = new QPushButton(tr("Observer tx deep link"), this);
    m_observerMint = new QPushButton(tr("Observer mint deep link"), this);
    m_observerTx->setEnabled(false);
    m_observerMint->setEnabled(false);
    m_observerTx->setToolTip(tr("ObserverClient integration point — not yet connected."));
    m_observerMint->setToolTip(tr("ObserverClient integration point — not yet connected."));
    layout->addWidget(m_observerTx);
    layout->addWidget(m_observerMint);
    layout->addStretch();
}

} // namespace RigorWallet
