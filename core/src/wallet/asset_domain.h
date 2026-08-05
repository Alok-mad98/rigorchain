// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_ASSET_DOMAIN_H
#define BITCOIN_WALLET_ASSET_DOMAIN_H

#include <primitives/transaction.h>

class CScript;

namespace wallet {

/** Witness-version vs selected asset (RFC-ADDRESS-001 / sendtoaddress logic). */
bool WitnessRFCBindingOk(const CScript& script_pub_key, AssetType send_asset);

/**
 * Full wallet send destination check: RFC witness binding plus script-inferred native asset
 * must match the selected asset (script-as-asset, same rule as listunspent categorization).
 *
 * W1.1D FREEZE INVARIANT (L2 enforcement layer): this is the canonical wallet-side
 * send-destination authorization checkpoint. Every send/receive RPC and GUI URI path
 * MUST pass through this primitive (or its caller-equivalent inline check in
 * rpcspend.cpp). Bypassing this check would enable silent ORU/RES confusion. Do NOT
 * introduce alternative send paths that skip this validation.
 */
bool WalletStrictSendDestinationOk(const CScript& script_pub_key, AssetType send_asset);

} // namespace wallet

#endif // BITCOIN_WALLET_ASSET_DOMAIN_H
