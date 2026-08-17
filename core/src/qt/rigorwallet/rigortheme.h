// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_RIGORTHEME_H
#define BITCOIN_QT_RIGORWALLET_RIGORTHEME_H

#include <QColor>
#include <QString>

namespace RigorWallet {

/** Color tokens aligned with Explorer MVP dark theme (black page, dark-gray surfaces). */
struct ColorPalette {
    QColor bgPrimary;       // #000000
    QColor bgSurface;       // #111111
    QColor bgElevated;      // #222222
    QColor borderSubtle;    // #333333
    QColor borderStrong;    // #444444
    QColor textPrimary;     // #FFFFFF
    QColor textSecondary;   // #FFFFFF
    QColor textMuted;       // #BBBBBB
    QColor textDisabled;    // #888888
    QColor accent;          // #FF6A2B
    QColor accentHover;     // #E85A20
    QColor link;            // #FFFFFF
    QColor success;         // #8FDC5A
    QColor warning;         // #FFB74D
    QColor critical;        // #FF6B6B
    QColor info;            // #8CC8FF
    QColor experimental;    // #BBBBBB
    QColor assetOru;        // #8CC8FF
    QColor assetOruBg;      // #8CC8FF14
    QColor assetRes;        // #8FDC5A
    QColor assetResBg;      // #8FDC5A14
    QColor convergence;     // #8CC8FF
    QColor convergenceBg;   // #8CC8FF14
};

/** Typography scale from PROGRAM-005 RIGOR_VISUAL_LANGUAGE. */
struct Typography {
    int uiBodyPx;           // 14
    int uiBodyLargePx;      // 16
    int headingPx;          // 20
    int dataMonoPx;         // 13
    int balancePx;          // 24
    int labelPx;            // 12
    int statusEnumPx;       // 12
    QString uiFontFamily;
    QString monoFontFamily;
};

/** Layout spacing from 8 px base grid. */
struct Spacing {
    int unit;               // 8
    int panelPadding;       // 12
    int cardPadding;        // 16
    int sectionGap;         // 16
    int bannerPadding;      // 10
    int borderRadiusPanel;  // 6
    int borderRadiusButton; // 6
    int borderRadiusChip;   // 4
    int assetBorderWidth;   // 3
};

/** Theme accessor — returns canonical dark-mode tokens. */
class RigorTheme
{
public:
    RigorTheme() = delete;

    static ColorPalette colors();
    static Typography typography();
    static Spacing spacing();

    /** Stylesheet fragment for a given hex background + foreground. */
    static QString bannerStylesheet(const QColor& bg, const QColor& fg, const QColor& border);

    static QString colorHex(const QColor& color);
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_RIGORTHEME_H
