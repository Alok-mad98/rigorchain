// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_EXPORT_H
#define BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_EXPORT_H

#include <QString>

class ClientModel;
class WalletModel;

namespace RigorWallet {

/** Read-only diagnostics bundle export (BUILD-34). */
class DiagnosticsExport
{
public:
    DiagnosticsExport() = delete;

    static bool exportBundle(const QString& directory,
                             ClientModel* clientModel,
                             WalletModel* walletModel,
                             QString* error_out = nullptr);
};

} // namespace RigorWallet

#endif // BITCOIN_QT_RIGORWALLET_DIAGNOSTICS_DIAGNOSTICS_EXPORT_H
