// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/asset_domain.h>

#include <consensus/res_owner.h>
#include <script/script.h>
#include <script/standard.h>

namespace wallet {
namespace {

/** True for the OP_RETURN RESO owner-declaration script used on ORU convergence txs (A-5). */
bool IsResOwnerDeclarationScriptPubKey(const CScript& script_pub_key)
{
    // Byte-identical shape check to Consensus::GetResOwnerDeclaration (single minimal push).
    static constexpr size_t kScriptSize = 2 + Consensus::RES_OWNER_PAYLOAD_SIZE;
    if (script_pub_key.size() != kScriptSize) return false;
    if (script_pub_key[0] != OP_RETURN) return false;
    if (script_pub_key[1] != static_cast<unsigned char>(Consensus::RES_OWNER_PAYLOAD_SIZE)) return false;
    const std::vector<unsigned char> payload(script_pub_key.begin() + 2, script_pub_key.end());
    return Consensus::ParseResOwnerDeclarationPayload(payload).has_value();
}

} // namespace

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
    // SECURITY-001-A5 companion: RESO OP_RETURN classifies as ASSET_NONE, but it is required
    // metadata on an ORU-domain convergence transaction (CreateConvergenceTransaction). Allow
    // it only when send_asset is ORU and the script is a well-formed declaration.
    if (send_asset == ASSET_ORU && IsResOwnerDeclarationScriptPubKey(script_pub_key)) {
        return true;
    }
    return AssetTypeFromScriptPubKey(script_pub_key) == send_asset;
}

} // namespace wallet
