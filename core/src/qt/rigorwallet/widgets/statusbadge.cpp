// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/widgets/statusbadge.h>

#include <qt/rigorwallet/rigortheme.h>

#include <QCoreApplication>

namespace RigorWallet {

StatusBadge::StatusBadge(QWidget* parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("RigorStatusBadge"));
    setStatus(StatusBadgeColor::Gray, labelForColor(StatusBadgeColor::Gray));
}

void StatusBadge::setColor(StatusBadgeColor color)
{
    m_color = color;
    applyStyle();
}

void StatusBadge::setStatus(StatusBadgeColor color, const QString& text)
{
    m_color = color;
    setText(text);
    applyStyle();
}

QString StatusBadge::labelForColor(StatusBadgeColor color)
{
    switch (color) {
    case StatusBadgeColor::Green:
        return QCoreApplication::translate("StatusBadge", "Ready");
    case StatusBadgeColor::Yellow:
        return QCoreApplication::translate("StatusBadge", "Check");
    case StatusBadgeColor::Red:
        return QCoreApplication::translate("StatusBadge", "Issue");
    case StatusBadgeColor::Gray:
        return QCoreApplication::translate("StatusBadge", "Unavailable");
    }
    return QCoreApplication::translate("StatusBadge", "Unavailable");
}

void StatusBadge::applyStyle()
{
    const ColorPalette p = RigorTheme::colors();
    QColor bg;
    QColor fg = p.textPrimary;
    switch (m_color) {
    case StatusBadgeColor::Green:
        bg = p.success;
        break;
    case StatusBadgeColor::Yellow:
        bg = p.warning;
        fg = QColor(QStringLiteral("#1A2332"));
        break;
    case StatusBadgeColor::Red:
        bg = p.critical;
        break;
    case StatusBadgeColor::Gray:
        bg = p.experimental;
        break;
    }
    setStyleSheet(QStringLiteral(
                      "QLabel#RigorStatusBadge {"
                      " background: %1; color: %2;"
                      " border-radius: 4px; padding: 2px 8px;"
                      " font-size: 11px; font-weight: 600; }")
                      .arg(RigorTheme::colorHex(bg), RigorTheme::colorHex(fg)));
}

} // namespace RigorWallet
