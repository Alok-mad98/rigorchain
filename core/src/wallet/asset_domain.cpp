// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/asset_domain.h>

#include <script/standard.h>

namespace wallet {

bool WalletStrictSendDestinationOk(const CScript& script_pub_key, AssetType send_asset)
{
    return AssetTypeFromScriptPubKey(script_pub_key) == send_asset;
}

} // namespace wallet
