// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/reuse_domain.h>

#include <script/standard.h>
#include <wallet/wallet.h>

namespace wallet {

bool OruReuseSemanticsApply(const CScript& script_pub_key)
{
    return AssetTypeFromScriptPubKey(script_pub_key) == ASSET_ORU;
}

bool OruReuseSemanticsApply(const AssetType asset)
{
    return asset == ASSET_ORU;
}

bool EffectiveAvoidReuseForAsset(const bool avoid_reuse, const AssetType asset)
{
    return avoid_reuse && OruReuseSemanticsApply(asset);
}

bool OutputBlockedByOruReuse(const CWallet& wallet, const CScript& script_pub_key, const bool allow_used_addresses)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);
    if (allow_used_addresses) {
        return false;
    }
    if (!OruReuseSemanticsApply(script_pub_key)) {
        return false;
    }
    return wallet.IsSpentKey(script_pub_key);
}

} // namespace wallet
