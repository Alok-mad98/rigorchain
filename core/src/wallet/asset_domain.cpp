// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/asset_domain.h>

#include <script/standard.h>

namespace wallet {

bool WitnessRFCBindingOk(const CScript& script_pub_key, AssetType send_asset)
{
    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (!script_pub_key.IsWitnessProgram(witnessversion, witnessprogram)) {
        return true;
    }
    // RFC-ADDRESS-001 / convergence: CVG2 is witness v1 but ORU-domain burn (AssetTypeFromScriptPubKey → ORU).
    if (send_asset == ASSET_ORU && witnessversion == 1 && IsConvergenceScript(script_pub_key)) {
        return true;
    }
    if (send_asset == ASSET_ORU && witnessversion != 0) {
        return false;
    }
    if (send_asset == ASSET_RES && witnessversion != 1) {
        return false;
    }
    // RES must never target a CVG2 burn script (ORU domain).
    if (send_asset == ASSET_RES && IsConvergenceScript(script_pub_key)) {
        return false;
    }
    return true;
}

bool WalletStrictSendDestinationOk(const CScript& script_pub_key, AssetType send_asset)
{
    if (!WitnessRFCBindingOk(script_pub_key, send_asset)) {
        return false;
    }
    return AssetTypeFromScriptPubKey(script_pub_key) == send_asset;
}

} // namespace wallet
