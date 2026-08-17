// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_AUTOSETTLE_CONVERGENCE_H
#define BITCOIN_QT_AUTOSETTLE_CONVERGENCE_H

class WalletModel;

/**
 * When OptionsModel::AutoSettleConvergenceMining is enabled, ensure
 * -miningresfeeaddr is set to a wallet witness-v1 RES address so CreateNewBlock
 * can assemble Convergence winner mints.
 *
 * Network fees remain ORU-only. The Convergence prize pays the winner — this
 * only arms mint assembly (legacy arg name miningresfeeaddr).
 *
 * @return true if assembly is armed after this call
 */
bool EnsureConvergenceMintArmed(WalletModel* wallet_model);

#endif // BITCOIN_QT_AUTOSETTLE_CONVERGENCE_H
