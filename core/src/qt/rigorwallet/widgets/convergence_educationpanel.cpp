// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_educationpanel.h>
#include <qt/rigorwallet/convergence/convergence_education.h>
#include <qt/rigorwallet/rigortheme.h>

#include <QTextBrowser>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceEducationPanel::ConvergenceEducationPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceEducation"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding,
                               RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding);

    auto* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setHtml(ConvergenceEducation::fullHelpHtml());
    browser->setMaximumHeight(220);
    layout->addWidget(browser);
}

} // namespace RigorWallet
