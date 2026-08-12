// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/convergence_educationpanel.h>
#include <qt/rigorwallet/convergence/convergence_education.h>
#include <qt/rigorwallet/rigortheme.h>

#include <QTextEdit>
#include <QVBoxLayout>

namespace RigorWallet {

ConvergenceEducationPanel::ConvergenceEducationPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RigorConvergenceEducation"));
    const ColorPalette p = RigorTheme::colors();
    // Match RES CONVERGENCE (node) / overview panel surface (#1A2332).
    setStyleSheet(QStringLiteral(
                      "QFrame#RigorConvergenceEducation {"
                      "  background: %1;"
                      "  border: 1px solid %2;"
                      "  border-radius: 6px;"
                      "}"
                      "QFrame#RigorConvergenceEducation QTextEdit {"
                      "  background: %1;"
                      "  color: %3;"
                      "  border: none;"
                      "  padding: 4px;"
                      "}")
                      .arg(RigorTheme::colorHex(p.bgSurface),
                           RigorTheme::colorHex(p.borderSubtle),
                           RigorTheme::colorHex(p.textPrimary)));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding,
                               RigorTheme::spacing().cardPadding, RigorTheme::spacing().panelPadding);

    auto* browser = new QTextEdit(this);
    browser->setReadOnly(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(ConvergenceEducation::fullHelpHtml());
    browser->setMaximumHeight(220);
    layout->addWidget(browser);
}

} // namespace RigorWallet
