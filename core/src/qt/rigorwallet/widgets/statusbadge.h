// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_WIDGETS_STATUSBADGE_H
#define BITCOIN_QT_RIGORWALLET_WIDGETS_STATUSBADGE_H

#include <QLabel>
#include <QString>

namespace RigorWallet {

/** Standardized status colors (BUILD-35). */
enum class StatusBadgeColor {
    Green,
    Yellow,
    Red,
    Gray,
};

/** Compact status chip used across diagnostics panels. */
class StatusBadge : public QLabel
{
    Q_OBJECT

public:
    explicit StatusBadge(QWidget* parent = nullptr);

    StatusBadgeColor color() const { return m_color; }
    void setColor(StatusBadgeColor color);
    void setStatus(StatusBadgeColor color, const QString& text);

    static QString labelForColor(StatusBadgeColor color);

private:
    void applyStyle();

    StatusBadgeColor m_color{StatusBadgeColor::Gray};
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_WIDGETS_STATUSBADGE_H
