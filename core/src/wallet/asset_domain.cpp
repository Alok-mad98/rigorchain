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
        // OP_CONVERGE (and other non-witness locks) are classified by script, not witness version.
        return true;
    }
    if (send_asset == ASSET_ORU && witnessversion != 0) {
        return false;
    }
    if (send_asset == ASSET_RES && witnessversion != 1) {
        return false;
    }
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
