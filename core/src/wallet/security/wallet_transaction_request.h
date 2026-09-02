// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet-layer transaction request / intent.
// This structure is not ConsensusKernelInput and must never be one.
// Wallet is not a consensus authority.

#ifndef BITCOIN_WALLET_SECURITY_WALLET_TRANSACTION_REQUEST_H
#define BITCOIN_WALLET_SECURITY_WALLET_TRANSACTION_REQUEST_H

#include <cstdint>
#include <string>
#include <vector>

/** Registered wallet-visible asset tags. Classification authority remains
 *  ClassifyRigorTxout in the Transaction Kernel. UNKNOWN is never sendable. */
enum class WalletAssetType : uint8_t {
    ORU = 0,
    RES = 1,
    UNKNOWN = 255,
};

enum class WalletTxKind : uint8_t {
    TRANSFER = 0,
    SIGNING = 1,
    IMPORT = 2,
    RPC = 3,
};

/** Map production AssetType (ASSET_ORU=0, ASSET_RES=1) without including
 *  primitives/transaction.h. Any other tag is UNKNOWN (not sendable). */
inline WalletAssetType WalletAssetFromProduction(uint8_t production_asset)
{
    if (production_asset == 0) return WalletAssetType::ORU;
    if (production_asset == 1) return WalletAssetType::RES;
    return WalletAssetType::UNKNOWN;
}

inline bool WalletAssetIsRegistered(WalletAssetType a)
{
    return a == WalletAssetType::ORU || a == WalletAssetType::RES;
}

struct WalletOutputSpec {
    int64_t amount{0};
    WalletAssetType asset{WalletAssetType::UNKNOWN};
    std::vector<uint8_t> destination;
};

struct WalletInputSpec {
    uint8_t prev_txid[32]{};
    uint32_t n{0};
    int64_t amount{0};
    WalletAssetType asset{WalletAssetType::UNKNOWN};
    /** 0 = not a maturity-gated coin. Wallet policy only; spend validity
     *  still enters VerifyRigorPredicate. */
    int64_t maturity_height{0};
    int64_t current_height{0};
};

/**
 * WalletTransactionRequest / WalletIntent.
 *
 * Forbidden contents (enforced by lint, not by this type):
 *   UTXO mutation, ledger apply, subsidy, RES reward, draw, supply delta,
 *   ClassifyRigorTxout body, VerifyRigorPredicate body.
 */
struct WalletTransactionRequest {
    WalletTxKind kind{WalletTxKind::TRANSFER};
    std::vector<WalletInputSpec> inputs;
    std::vector<WalletOutputSpec> outputs;
    int64_t available_balance{-1}; // <0 means "coin selection later"
    bool inputs_provided{false};
};

/** Validated intent. Same layout; produced only after Evaluate accepts. */
using WalletIntent = WalletTransactionRequest;

struct WalletRpcRequest {
    std::string method;
    std::vector<std::string> params;
    uint64_t raw_size{0};
};

struct WalletImportRequest {
    std::vector<uint8_t> material;
    std::string format;
};

struct WalletSigningRequest {
    std::vector<uint8_t> canonical_tx;
    uint8_t digest[32]{};
    uint64_t key_id{0};
    uint8_t secret[64]{};
    bool secret_set{false};
    /** Wallet must not modify production sighash / witness / flags. */
    bool digest_layout_locked{true};
    bool witness_format_locked{true};
    bool signature_flags_locked{true};
};

#endif // BITCOIN_WALLET_SECURITY_WALLET_TRANSACTION_REQUEST_H
