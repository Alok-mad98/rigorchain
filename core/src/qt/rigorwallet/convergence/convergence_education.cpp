// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/convergence/convergence_education.h>

#include <QCoreApplication>

namespace RigorWallet {

QString ConvergenceEducation::whatIsOru()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "ORU is the mined base asset on RIGOR. You earn ORU from mining or receive it from others. "
        "Network fees are always paid in ORU.");
}

QString ConvergenceEducation::whatIsRes()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "RES is a derived asset that may be minted through protocol rules. RES is not guaranteed per block "
        "and requires maturity before it becomes spendable.");
}

QString ConvergenceEducation::whatIsConvergence()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "Convergence permanently commits ORU toward the protocol draw. It does not guarantee RES. "
        "The node — not the wallet — determines selection outcomes.");
}

QString ConvergenceEducation::whySeparate()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "ORU and RES are separate native assets with distinct scripts and balances. "
        "The wallet never mixes them in a single transaction or combined balance display.");
}

QString ConvergenceEducation::fullHelpHtml()
{
    return QStringLiteral(
        "<h3>%1</h3><p>%2</p>"
        "<h3>%3</h3><p>%4</p>"
        "<h3>%5</h3><p>%6</p>"
        "<h3>%7</h3><p>%8</p>")
        .arg(QCoreApplication::translate("ConvergenceEducation", "What is ORU?"), whatIsOru(),
             QCoreApplication::translate("ConvergenceEducation", "What is RES?"), whatIsRes(),
             QCoreApplication::translate("ConvergenceEducation", "What is Convergence?"), whatIsConvergence(),
             QCoreApplication::translate("ConvergenceEducation", "Why are they separate?"), whySeparate());
}

} // namespace RigorWallet
