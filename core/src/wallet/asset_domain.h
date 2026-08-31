// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_ASSET_DOMAIN_H
#define BITCOIN_WALLET_ASSET_DOMAIN_H

#include <primitives/transaction.h>

class CScript;

namespace wallet {

/**
 * Wallet send destination check: AssetTypeFromScriptPubKey(script) must equal
 * send_asset. Never classify by witness version, address type, or descriptor.
 */
bool WalletStrictSendDestinationOk(const CScript& script_pub_key, AssetType send_asset);

} // namespace wallet

#endif // BITCOIN_WALLET_ASSET_DOMAIN_H
