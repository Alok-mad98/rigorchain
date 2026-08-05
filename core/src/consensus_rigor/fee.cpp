// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus_rigor/fee.h>

#include <coins.h>
#include <consensus/amount.h>
#include <consensus/tx_verify.h>
#include <consensus/convergenceres.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <serialize.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace rigor {

bool CalculateFee_ORUM(const CTransaction& tx, const CCoinsViewCache& inputs, uint64_t& fee_out)
{
    // RIGOR Fee Mechanism Specification §1.1: Fee Calculation (the sole rule)
    // fee_i = sum(inputs_ORUM) - sum(outputs_ORUM)
    //
    // Constraints:
    // - Unit: minimum unit (satoshi-like)
    // - Type: uint64 (unsigned 64-bit integer)
    // - fee_i MUST >= 0
    // - fee_i == 0 → transaction MUST NOT enter candidate set
    
    if (tx.IsCoinBase()) {
        fee_out = 0;
        return false;
    }

    // POOL-F: sum ORU inputs only (dual-pool RES spends may include RES prevouts).
    uint64_t inputs_orum = 0;
    bool has_orum_input = false;
    for (const auto& txin : tx.vin) {
        const Coin& coin = inputs.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            fee_out = 0;
            return false;
        }

        const AssetType asset = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (asset != ASSET_ORU) {
            continue;
        }
        has_orum_input = true;

        if (inputs_orum > UINT64_MAX - static_cast<uint64_t>(coin.out.nValue)) {
            fee_out = 0;
            return false;
        }
        inputs_orum += static_cast<uint64_t>(coin.out.nValue);
    }

    if (!has_orum_input) {
        fee_out = 0;
        return false;
    }
    
    // Calculate sum of outputs_ORUM
    uint64_t outputs_orum = 0;
    for (const auto& txout : tx.vout) {
        const AssetType asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
        if (asset == ASSET_ORU) {
            // Check for overflow
            if (outputs_orum > UINT64_MAX - static_cast<uint64_t>(txout.nValue)) {
                fee_out = 0;
                return false;
            }
            outputs_orum += static_cast<uint64_t>(txout.nValue);
        }
    }
    
    // fee_i = inputs_ORUM - outputs_ORUM
    if (inputs_orum < outputs_orum) {
        fee_out = 0;
        return false;
    }
    
    fee_out = inputs_orum - outputs_orum;
    return true;
}

size_t GetTransactionSizeBytes(const CTransaction& tx)
{
    // RIGOR Fee Mechanism Specification §1.2: Transaction Size Definition (the sole standard)
    // tx_size_bytes = serialized transaction size in bytes
    //
    // Requirements:
    // - Use network serialized byte length (wire format)
    // - FORBIDDEN: weight, virtual size, witness discount, any discount mechanisms
    // - size MUST be exact byte count
    
    // Use GetSerializeSize with PROTOCOL_VERSION to get exact wire format byte count
    // This is the canonical serialization size as used on the network
    return ::GetSerializeSize(tx, PROTOCOL_VERSION);
}

bool CompareFeeRate(uint64_t fee_a, size_t size_a, uint64_t fee_b, size_t size_b)
{
    // RIGOR Fee Mechanism Specification §1.3: Fee Rate Definition (ordering only; never enters consensus)
    // fee_rate_i = fee_i / tx_size_bytes
    //
    // Note:
    // - fee_rate is ONLY used for mempool / block packing strategy
    // - fee_rate NEVER enters consensus calculation
    // - FORBIDDEN floating point: use integer comparison (cross multiplication)
    //
    // RIGOR Fee Mechanism Specification §3.1: Mempool ordering rules
    // All nodes MUST sort by fee_rate from high to low
    // Comparison (floating point forbidden):
    //   a outranks b ⇔ (a.fee * b.size) > (b.fee * a.size)
    
    // Avoid division by using cross multiplication
    // fee_a / size_a > fee_b / size_b  <=>  fee_a * size_b > fee_b * size_a
    
    // Handle zero size (should not happen in valid transactions)
    if (size_a == 0 || size_b == 0) {
        return false;
    }
    
    // Use 128-bit multiplication to avoid overflow
    // fee_a * size_b
    unsigned __int128 left = static_cast<unsigned __int128>(fee_a) * static_cast<unsigned __int128>(size_b);
    // fee_b * size_a
    unsigned __int128 right = static_cast<unsigned __int128>(fee_b) * static_cast<unsigned __int128>(size_a);
    
    return left > right;
}

bool CalculateF_i(const CTransaction& tx, const CCoinsViewCache& inputs, uint64_t& f_i_out)
{
    // RIGOR Fee Mechanism Specification §2: f_i Definition (consensus binding, absolutely core)
    // f_i =
    //     if RES Mint:   Convergen_amount (ORUM)
    //     if RES Spend:  fee_i (ORUM)
    //
    // §2.1 Mandatory constraints
    // - f_i MUST > 0
    // - f_i MUST be uint64
    // - f_i MUST directly enter consensus inequality
    // - FORBIDDEN any transformation: log(f_i), f_i^β, scaling, normalization
    
    if (tx.IsCoinBase()) {
        f_i_out = 0;
        return false;
    }

    // Block-entry views (e.g. ComputeConvergenceDrawCandidateRoot before UpdateCoins) do not
    // include outputs of earlier txs in this block. Skip until inputs exist on the view.
    if (!inputs.HaveInputs(tx)) {
        f_i_out = 0;
        return false;
    }

    // Determine if this is RES Mint or RES Spend
    const std::optional<bool> is_mint = IsRESMintTx(tx, inputs);
    if (!is_mint.has_value()) {
        f_i_out = 0;
        return false;
    }
    
    if (*is_mint) {
        // RES Mint: f_i = Convergen_amount (ORUM)
        const CAmount convergence_amount = Consensus::GetConvergenceAmount(tx);
        if (convergence_amount <= 0) {
            f_i_out = 0;
            return false;
        }
        f_i_out = static_cast<uint64_t>(convergence_amount);
    } else {
        // RES Spend: f_i = fee_i (ORUM)
        if (!CalculateFee_ORUM(tx, inputs, f_i_out)) {
            return false;
        }
    }
    
    // f_i MUST > 0
    if (f_i_out == 0) {
        return false;
    }
    
    return true;
}

std::optional<bool> IsRESMintTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    // RIGOR Fee Mechanism Specification §2: f_i Definition (consensus binding)
    // Helper function to determine if transaction is RES Mint or RES Spend
    
    if (tx.IsCoinBase()) {
        return std::nullopt;
    }
    
    // Check input asset domain
    const std::optional<AssetType> input_asset = Consensus::GetTransactionInputAsset(tx, inputs);
    if (!input_asset.has_value()) {
        return std::nullopt;
    }
    
    // RES Mint: inputs are ORUM, outputs include RES
    // RES Spend: inputs are RES, outputs are RES
    
    if (*input_asset == ASSET_ORU) {
        // Check if any output is RES
        for (const auto& txout : tx.vout) {
            const AssetType out_asset = AssetTypeFromScriptPubKey(txout.scriptPubKey);
            if (out_asset == ASSET_RES) {
                return true; // RES Mint
            }
        }
        return false; // ORUM transaction, not RES Mint
    } else if (*input_asset == ASSET_RES) {
        return false; // RES Spend
    }
    
    return std::nullopt;
}

bool IsORUMFeeTx(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase()) {
        return false;
    }

    uint64_t fee_dummy = 0;
    return CalculateFee_ORUM(tx, inputs, fee_dummy);
}

} // namespace rigor
