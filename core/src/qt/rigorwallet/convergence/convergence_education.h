// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_CONVERGENCE_EDUCATION_H
#define BITCOIN_QT_RIGORWALLET_CONVERGENCE_EDUCATION_H

#include <QString>

namespace RigorWallet {

/** Wallet-native educational copy (BUILD-24) — presentation only. */
class ConvergenceEducation
{
public:
    ConvergenceEducation() = delete;

    static QString whatIsOru();
    static QString whatIsRes();
    static QString whatIsConvergence();
    static QString whySeparate();
    static QString fullHelpHtml();
};

} // namespace RigorWallet

#endif
