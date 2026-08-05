// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_RIGORTHEME_H
#define BITCOIN_QT_RIGORWALLET_RIGORTHEME_H

#include <QColor>
#include <QString>

namespace RigorWallet {

/** Color tokens from PROGRAM-005 RIGOR_COLOR_SYSTEM (dark mode default). */
struct ColorPalette {
    QColor bgPrimary;       // #0F1419
    QColor bgSurface;       // #1A2332
    QColor bgElevated;      // #222830
    QColor borderSubtle;    // #2C3544
    QColor borderStrong;    // #3D4A5C
    QColor textPrimary;     // #E8EDF4
    QColor textSecondary;   // #B8C4D4
    QColor textMuted;       // #8B9CB3
    QColor textDisabled;    // #5C6B7F
    QColor accent;          // #FF6A2B
    QColor accentHover;     // #E85A20
    QColor link;            // #5B9FD4
    QColor success;         // #22C55E
    QColor warning;         // #F59E0B
    QColor critical;        // #EF4444
    QColor info;            // #5B9FD4
    QColor experimental;    // #8B9CB3
    QColor assetOru;        // #94A3B8
    QColor assetOruBg;      // #94A3B814
    QColor assetRes;        // #A78BFA
    QColor assetResBg;      // #A78BFA14
    QColor convergence;     // #5B9FD4
    QColor convergenceBg;   // #5B9FD414
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
