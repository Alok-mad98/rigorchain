// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rigorwallet/convergence/convergence_education.h>

#include <QCoreApplication>

namespace RigorWallet {

QString ConvergenceEducation::whatIsOru()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "ORU provides PoW security and economic coordination. You earn ORU from mining or receive it from others. "
        "Network fees and RES CONVERGENCE participation costs are paid in ORU.");
}

QString ConvergenceEducation::whatIsRes()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "RES is an independent scarce resource. RES is not a mining reward. "
        "RES is minted through deterministic consensus Convergence under a fixed maximum supply.");
}

QString ConvergenceEducation::whatIsConvergence()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "Convergence permanently commits ORU to participate in RES Convergence. "
        "Your ORU participates during the next 32 blocks before the network determines the result. "
        "The network — not the wallet — determines the result. Convergence does not guarantee RES.");
}

QString ConvergenceEducation::whySeparate()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "ORU is the PoW asset. RES is the scarce resource created through Convergence. "
        "They use distinct witness versions and balances. The wallet never mixes them in one transaction or combined total.");
}

QString ConvergenceEducation::whatIsPowOrdering()
{
    return QCoreApplication::translate("ConvergenceEducation",
        "Proof-of-work provides the security ordering that enables deterministic RES Convergence. "
        "PoW does not directly create RES.");
}

QString ConvergenceEducation::fullHelpHtml()
{
    return QStringLiteral(
        "<h3>%1</h3><p>%2</p>"
        "<h3>%3</h3><p>%4</p>"
        "<h3>%5</h3><p>%6</p>"
        "<h3>%7</h3><p>%8</p>"
        "<h3>%9</h3><p>%10</p>")
        .arg(QCoreApplication::translate("ConvergenceEducation", "What is ORU?"), whatIsOru(),
             QCoreApplication::translate("ConvergenceEducation", "What is RES?"), whatIsRes(),
             QCoreApplication::translate("ConvergenceEducation", "What is Convergence?"), whatIsConvergence(),
             QCoreApplication::translate("ConvergenceEducation", "Why are they separate?"), whySeparate(),
             QCoreApplication::translate("ConvergenceEducation", "Role of proof-of-work"), whatIsPowOrdering());
}

} // namespace RigorWallet
