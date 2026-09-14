// Copyright (c) 2011-2021 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinunits.h>

#include <consensus/amount.h>
#include <consensus/res_amount.h>

#include <QStringList>

#include <cassert>

static constexpr auto MAX_DIGITS_ORU = 16;

static_assert(COIN / 1000 == 100'000, "mORU factor must match COIN/1000 (100000 base units per mORU)");

BitcoinUnits::BitcoinUnits(QObject *parent):
        QAbstractListModel(parent),
        unitlist(availableUnits())
{
}

QList<BitcoinUnit> BitcoinUnits::availableUnits()
{
    QList<BitcoinUnit> unitlist;
    unitlist.append(Unit::ORU);
    unitlist.append(Unit::mORU);
    unitlist.append(Unit::uORU);
    unitlist.append(Unit::THARN);
    return unitlist;
}

QString BitcoinUnits::longName(Unit unit)
{
    switch (unit) {
    case Unit::ORU: return QStringLiteral("ORU");
    case Unit::mORU: return QStringLiteral("mORU");
    case Unit::uORU: return QString::fromUtf8("µORU (bits)");
    case Unit::THARN: return QStringLiteral("Tharn");
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

QString BitcoinUnits::shortName(Unit unit)
{
    switch (unit) {
    case Unit::ORU: return longName(unit);
    case Unit::mORU: return longName(unit);
    case Unit::uORU: return QStringLiteral("bits");
    case Unit::THARN: return QStringLiteral("tharn");
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

QString BitcoinUnits::description(Unit unit)
{
    switch (unit) {
    case Unit::ORU: return QStringLiteral("ORU");
    case Unit::mORU: return QStringLiteral("Milli-ORU (1" THIN_SP_UTF8 "/ 1" THIN_SP_UTF8 "000 ORU)");
    case Unit::uORU: return QStringLiteral("Micro-ORU (bits) (1" THIN_SP_UTF8 "/ 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000 ORU)");
    case Unit::THARN: return QStringLiteral("Tharn (1" THIN_SP_UTF8 "/ 100" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000 ORU)");
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

qint64 BitcoinUnits::factor(Unit unit)
{
    switch (unit) {
    case Unit::ORU: return COIN;
    case Unit::mORU: return COIN / 1000;
    case Unit::uORU: return COIN / 1'000'000;
    case Unit::THARN: return 1;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

int BitcoinUnits::decimals(Unit unit)
{
    switch (unit) {
    case Unit::ORU: return 8;
    case Unit::mORU: return 5;
    case Unit::uORU: return 2;
    case Unit::THARN: return 0;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

QString BitcoinUnits::format(Unit unit, const CAmount& nIn, bool fPlus, SeparatorStyle separators, bool justify)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    qint64 n = (qint64)nIn;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    QString quotient_str = QString::number(quotient);
    if (justify) {
        quotient_str = quotient_str.rightJustified(MAX_DIGITS_ORU - num_decimals, ' ');
    }

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == SeparatorStyle::ALWAYS || (separators == SeparatorStyle::STANDARD && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');

    if (num_decimals > 0) {
        qint64 remainder = n_abs % coin;
        QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');
        return quotient_str + QString(".") + remainder_str;
    } else {
        return quotient_str;
    }
}


// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString BitcoinUnits::formatWithUnit(Unit unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    return format(unit, amount, plussign, separators) + QString(" ") + shortName(unit);
}

QString BitcoinUnits::formatHtmlWithUnit(Unit unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

QString BitcoinUnits::formatWithPrivacy(Unit unit, const CAmount& amount, SeparatorStyle separators, bool privacy)
{
    assert(amount >= 0);
    QString value;
    if (privacy) {
        value = format(unit, 0, false, separators, true).replace('0', '#');
    } else {
        value = format(unit, amount, false, separators, true);
    }
    return value + QString(" ") + shortName(unit);
}

QString BitcoinUnits::formatRes(const CAmount& amount, bool plussign, SeparatorStyle separators, bool justify)
{
    qint64 n = (qint64)amount;
    const qint64 coin = RES_COIN;
    const int num_decimals = 8;
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    QString quotient_str = QString::number(quotient);
    if (justify) {
        quotient_str = quotient_str.rightJustified(MAX_DIGITS_ORU - num_decimals, ' ');
    }
    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == SeparatorStyle::ALWAYS || (separators == SeparatorStyle::STANDARD && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (plussign && n > 0)
        quotient_str.insert(0, '+');

    qint64 remainder = n_abs % coin;
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');
    return quotient_str + QString(".") + remainder_str;
}

QString BitcoinUnits::formatResWithPrivacy(const CAmount& amount, SeparatorStyle separators, bool privacy)
{
    assert(amount >= 0);
    QString value;
    if (privacy) {
        value = formatRes(0, false, separators, true).replace('0', '#');
    } else {
        value = formatRes(amount, false, separators, true);
    }
    return value + QString(" RES");
}

QString BitcoinUnits::formatAmountForAsset(AssetType asset, const CAmount& amount, Unit oru_display_unit,
    bool plussign, SeparatorStyle separators, bool with_unit_suffix)
{
    if (asset == ASSET_RES) {
        const QString base = formatRes(amount, plussign, separators);
        return with_unit_suffix ? base + QStringLiteral(" RES") : base;
    }
    if (with_unit_suffix) {
        return formatWithUnit(oru_display_unit, amount, plussign, separators);
    }
    return format(oru_display_unit, amount, plussign, separators);
}

QString BitcoinUnits::formatAmountHtmlForAsset(AssetType asset, const CAmount& amount, Unit oru_display_unit,
    bool plussign, SeparatorStyle separators)
{
    QString str(formatAmountForAsset(asset, amount, oru_display_unit, plussign, separators, true));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

QString BitcoinUnits::formatFeeOru(const CAmount& fee_oru, Unit oru_display_unit, bool plussign, SeparatorStyle separators)
{
    return formatWithUnit(oru_display_unit, fee_oru, plussign, separators);
}

QString BitcoinUnits::formatFeeOruHtml(const CAmount& fee_oru, Unit oru_display_unit, bool plussign, SeparatorStyle separators)
{
    return formatHtmlWithUnit(oru_display_unit, fee_oru, plussign, separators);
}

bool BitcoinUnits::parseRes(const QString& value, CAmount* val_out)
{
    if (value.isEmpty()) {
        return false;
    }
    const int num_decimals = 8;
    QStringList parts = removeSpaces(value).split(".");
    if (parts.size() > 2) {
        return false;
    }
    QString whole = parts[0];
    QString decimals;
    if (parts.size() > 1) {
        decimals = parts[1];
    }
    if (decimals.size() > num_decimals) {
        return false;
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');
    if (str.size() > 18) {
        return false;
    }
    CAmount retvalue(str.toLongLong(&ok));
    if (val_out) {
        *val_out = retvalue;
    }
    return ok;
}

bool BitcoinUnits::parse(Unit unit, const QString& value, CAmount* val_out)
{
    if (value.isEmpty()) {
        return false; // Refuse to parse invalid unit or empty string
    }
    int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if(parts.size() > 2)
    {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > num_decimals)
    {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if(str.size() > 18)
    {
        return false; // Longer numbers will exceed 63 bits
    }
    CAmount retvalue(str.toLongLong(&ok));
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

QString BitcoinUnits::getAmountColumnTitle(Unit unit)
{
    return QObject::tr("Amount") + " (" + shortName(unit) + ")";
}

int BitcoinUnits::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant BitcoinUnits::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if(row >= 0 && row < unitlist.size())
    {
        Unit unit = unitlist.at(row);
        switch(role)
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(longName(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant::fromValue(unit);
        }
    }
    return QVariant();
}

CAmount BitcoinUnits::maxMoney()
{
    return MAX_MONEY;
}

namespace {
qint8 ToQint8(BitcoinUnit unit)
{
    switch (unit) {
    case BitcoinUnit::ORU: return 0;
    case BitcoinUnit::mORU: return 1;
    case BitcoinUnit::uORU: return 2;
    case BitcoinUnit::THARN: return 3;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

BitcoinUnit FromQint8(qint8 num)
{
    switch (num) {
    case 0: return BitcoinUnit::ORU;
    case 1: return BitcoinUnit::mORU;
    case 2: return BitcoinUnit::uORU;
    case 3: return BitcoinUnit::THARN;
    }
    assert(false);
}
} // namespace

QDataStream& operator<<(QDataStream& out, const BitcoinUnit& unit)
{
    return out << ToQint8(unit);
}

QDataStream& operator>>(QDataStream& in, BitcoinUnit& unit)
{
    qint8 input;
    in >> input;
    unit = FromQint8(input);
    return in;
}
