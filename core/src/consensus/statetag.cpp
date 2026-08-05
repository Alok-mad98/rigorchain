// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/statetag.h>
#include <coins.h>
#include <primitives/transaction.h>
#include <script/script.h>

/**
 * RFC-003: StateTag and Asset State Model
 * Transaction Classification and State Transition Validation
 */

namespace Consensus {

// ---------------------------------------------------------------------------
// RFC-003 ?6: Transaction Classification
// ---------------------------------------------------------------------------

/**
 * RFC-003 ?6.1: RES Mint Transaction
 * A transaction is RES Mint iff:
 * - input asset == ORU
 * - AND contains RES output
 * - AND contains valid PCRES witness
 * - AND contains valid Converge
 */
bool IsRESMintTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    // Check if transaction has inputs
    if (tx.vin.empty()) {
        return false;
    }

    // Check if transaction has outputs
    if (tx.vout.empty()) {
        return false;
    }

    // Check input asset type from UTXO set
    uint8_t input_asset = GetInputAsset(tx, inputs);
    if (input_asset != ASSET_ID_ORU) {
        return false;
    }

    // Check if any output is RES
    bool has_res_output = false;
    for (const auto& out : tx.vout) {
        AssetType asset = AssetTypeFromScriptPubKey(out.scriptPubKey);
        if (asset == AssetType::ASSET_RES) {
            has_res_output = true;
            break;
        }
    }

    if (!has_res_output) {
        return false;
    }

    // NOTE: Full validation also requires:
    // - Valid PCRES witness (VerifyConvergeProof)
    // - Valid Converge (Converge output present and valid)

    return true;
}

/**
 * RFC-003 ?6.2: RES Spend Transaction
 * A transaction is RES Spend iff:
 * - input asset == RES
 */
bool IsRESSpendTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    // Check if transaction has inputs
    if (tx.vin.empty()) {
        return false;
    }

    // Check input asset type from UTXO set
    uint8_t input_asset = GetInputAsset(tx, inputs);
    if (input_asset != ASSET_ID_RES) {
        return false;
    }

    return true;
}

/**
 * RFC-003 ?6.3: ORU Transfer
 * A transaction is ORU Transfer iff:
 * - input asset == ORU
 * - AND no RES output
 */
bool IsORUTransferTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    // Check if transaction has inputs
    if (tx.vin.empty()) {
        return false;
    }

    // Check input asset type from UTXO set
    uint8_t input_asset = GetInputAsset(tx, inputs);
    if (input_asset != ASSET_ID_ORU) {
        return false;
    }

    // Check if no RES output exists
    for (const auto& out : tx.vout) {
        AssetType asset = AssetTypeFromScriptPubKey(out.scriptPubKey);
        if (asset == AssetType::ASSET_RES) {
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// RFC-003 ?7: State Transition Rules
// ---------------------------------------------------------------------------

/**
 * RFC-003 ?7.5: Forbidden Transitions
 * Forbidden:
 * - RES -> ORU
 * - ORU -> RES (except mint)
 * - multiple mint
 * - negative amount
 * - unknown state
 */
bool IsValidStateTransition(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    // Check for negative amounts
    for (const auto& out : tx.vout) {
        if (out.nValue < 0) {
            return false;
        }
    }

    // Get input asset type
    uint8_t input_asset = GetInputAsset(tx, inputs);
    if (input_asset == 0) {
        // Mixed assets or invalid
        return false;
    }

    // Check output asset types
    bool has_oru_output = false;
    bool has_res_output = false;
    for (const auto& out : tx.vout) {
        AssetType asset = AssetTypeFromScriptPubKey(out.scriptPubKey);
        if (asset == AssetType::ASSET_ORU) {
            has_oru_output = true;
        } else if (asset == AssetType::ASSET_RES) {
            has_res_output = true;
        }
    }

    // Forbidden: RES -> ORU
    if (input_asset == ASSET_ID_RES && has_oru_output) {
        return false;
    }

    // Forbidden: ORU -> RES (except mint)
    if (input_asset == ASSET_ID_ORU && has_res_output && !IsRESMintTx(tx, inputs)) {
        return false;
    }

    // NOTE: Additional checks needed:
    // - Verifying no multiple mints in single transaction
    // - Verifying StateTag types are valid

    return true;
}

// ---------------------------------------------------------------------------
// RFC-003 ?10: Validation Engine - UTXO Access
// ---------------------------------------------------------------------------

/**
 * RFC-003 ?10: GetInputAsset ? semantic domain (RES prevouts dominate; ORU fee inputs do not flip semantics).
 */
uint8_t GetInputAsset(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.vin.empty()) {
        return 0;
    }

    bool has_oru = false;
    bool has_res = false;

    for (const auto& input : tx.vin) {
        Coin coin;
        if (!inputs.GetCoin(input.prevout, coin)) {
            return 0;
        }

        const AssetType asset = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (asset == AssetType::ASSET_ORU) {
            has_oru = true;
        } else if (asset == AssetType::ASSET_RES) {
            has_res = true;
        } else {
            return 0;
        }
    }

    if (has_res) {
        return ASSET_ID_RES;
    }
    if (has_oru) {
        return ASSET_ID_ORU;
    }
    return 0;
}

} // namespace Consensus
