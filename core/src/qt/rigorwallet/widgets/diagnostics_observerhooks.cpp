// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/diagnostics_observerhooks.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace RigorWallet {

DiagnosticsObserverHooks::DiagnosticsObserverHooks(QWidget* parent)
    : QFrame(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->addWidget(new QLabel(tr("Observer (future):"), this));
    m_openObserver = new QPushButton(tr("Open observer dashboard"), this);
    m_exportObserver = new QPushButton(tr("Export observer snapshot"), this);
    m_openObserver->setEnabled(false);
    m_exportObserver->setEnabled(false);
    m_openObserver->setToolTip(tr("ObserverClient integration point — not yet connected."));
    m_exportObserver->setToolTip(tr("ObserverClient integration point — not yet connected."));
    layout->addWidget(m_openObserver);
    layout->addWidget(m_exportObserver);
    layout->addStretch();
}

} // namespace RigorWallet
