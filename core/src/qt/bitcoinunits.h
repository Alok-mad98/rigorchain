// Copyright (c) 2011-2021 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINUNITS_H
#define BITCOIN_QT_BITCOINUNITS_H

#include <consensus/amount.h>
#include <primitives/transaction.h>

#include <QAbstractListModel>
#include <QDataStream>
#include <QString>

// U+2009 THIN SPACE = UTF-8 E2 80 89
#define REAL_THIN_SP_CP 0x2009
#define REAL_THIN_SP_UTF8 "\xE2\x80\x89"

// QMessageBox seems to have a bug whereby it doesn't display thin/hair spaces
// correctly.  Workaround is to display a space in a small font.  If you
// change this, please test that it doesn't cause the parent span to start
// wrapping.
#define HTML_HACK_SP "<span style='white-space: nowrap; font-size: 6pt'> </span>"

// Define THIN_SP_* variables to be our preferred type of thin space
#define THIN_SP_CP   REAL_THIN_SP_CP
#define THIN_SP_UTF8 REAL_THIN_SP_UTF8
#define THIN_SP_HTML HTML_HACK_SP

/** Native ORU unit definitions (display). Base amount is COIN=1e-8 ORU per consensus; smallest display unit: Tharn.
   Encapsulates parsing and formatting and serves as list model for drop-down selection boxes.
*/
class BitcoinUnits: public QAbstractListModel
{
    Q_OBJECT

public:
    explicit BitcoinUnits(QObject *parent);

    /** Display units for ORU-denominated amounts (internal storage unchanged: CAmount in base units). */
    enum class Unit {
        ORU,  //!< 1 ORU = COIN base units
        mORU,  //!< 1 mORU = 1/1000 ORU = 100 000 base units
        uORU, //!< micro-ORU ("bits") = 1/1 000 000 ORU = 100 base units
        THARN //!< 1 Tharn = 1 base unit = 1e-8 ORU
    };
    Q_ENUM(Unit)

    enum class SeparatorStyle
    {
        NEVER,
        STANDARD,
        ALWAYS
    };

    //! @name Static API
    //! Unit conversion and formatting
    ///@{

    //! Get list of units, for drop-down box
    static QList<Unit> availableUnits();
    //! Long name
    static QString longName(Unit unit);
    //! Short name
    static QString shortName(Unit unit);
    //! Longer description
    static QString description(Unit unit);
    //! Number of base units (1e-8 ORU each) per one display unit
    static qint64 factor(Unit unit);
    //! Number of decimals left
    static int decimals(Unit unit);
    //! Format as string
    static QString format(Unit unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD, bool justify = false);
    //! Format as string (with unit)
    static QString formatWithUnit(Unit unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD);
    //! Format as HTML string (with unit)
    static QString formatHtmlWithUnit(Unit unit, const CAmount& amount, bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD);
    //! Format as string (with unit) of fixed length to preserve privacy, if it is set.
    static QString formatWithPrivacy(Unit unit, const CAmount& amount, SeparatorStyle separators, bool privacy);
    /** Res native amounts: internal scale RES_COIN=1e8 per 1.00000000 RES (8 decimal places). */
    static QString formatRes(const CAmount& amount, bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD, bool justify = false);
    static bool parseRes(const QString& value, CAmount* val_out);
    static QString formatResWithPrivacy(const CAmount& amount, SeparatorStyle separators, bool privacy);
    /** Format a transfer amount in its native asset domain (ORU display unit is subdivision only). */
    static QString formatAmountForAsset(AssetType asset, const CAmount& amount, Unit oru_display_unit,
        bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD, bool with_unit_suffix = false);
    static QString formatAmountHtmlForAsset(AssetType asset, const CAmount& amount, Unit oru_display_unit,
        bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD);
    /** Network fee is always ORU (8-decimal domain); never use transfer-asset formatters for fees. */
    static QString formatFeeOru(const CAmount& fee_oru, Unit oru_display_unit,
        bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD);
    static QString formatFeeOruHtml(const CAmount& fee_oru, Unit oru_display_unit,
        bool plussign = false, SeparatorStyle separators = SeparatorStyle::STANDARD);
    //! Parse string to coin amount
    static bool parse(Unit unit, const QString& value, CAmount* val_out);
    //! Gets title for amount column including current display unit if optionsModel reference available */
    static QString getAmountColumnTitle(Unit unit);
    ///@}

    //! @name AbstractListModel implementation
    //! List model for unit drop-down selection box.
    ///@{
    enum RoleIndex {
        /** Unit identifier */
        UnitRole = Qt::UserRole
    };
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    ///@}

    static QString removeSpaces(QString text)
    {
        text.remove(' ');
        text.remove(QChar(THIN_SP_CP));
        return text;
    }

    //! Return maximum number of base units (Tharn)
    static CAmount maxMoney();

private:
    QList<Unit> unitlist;
};
typedef BitcoinUnits::Unit BitcoinUnit;

QDataStream& operator<<(QDataStream& out, const BitcoinUnit& unit);
QDataStream& operator>>(QDataStream& in, BitcoinUnit& unit);

#endif // BITCOIN_QT_BITCOINUNITS_H
