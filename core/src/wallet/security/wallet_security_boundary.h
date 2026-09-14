// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet request → construction boundary.
// Output is only ACCEPT_TO_BUILDER / REJECT.
//
// Pipeline:
//   WalletIntent → TransactionBuilder → Canonical Transaction → TransactionKernel
//
// This module is a wallet firewall. It does not:
//   - modify UTXO / ledger state
//   - call GetBlockSubsidy / ResolveConvergenceDraw / RigorSupplyDelta /
//     LedgerApply
//   - implement ClassifyRigorTxout or VerifyRigorPredicate
//   - emit ConsensusKernelInput
//   - decide spend validity or asset classification
//
// Final spend validity of an accepted transaction MUST enter
// VerifyRigorPredicate at the Transaction Kernel boundary.

#ifndef BITCOIN_WALLET_SECURITY_WALLET_SECURITY_BOUNDARY_H
#define BITCOIN_WALLET_SECURITY_WALLET_SECURITY_BOUNDARY_H

#include <wallet/security/wallet_security_result.h>
#include <wallet/security/wallet_transaction_request.h>

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * Wallet-layer ceilings. Numeric values MUST match production constants:
 *   MAX_MONEY (consensus/amount.h) = 10_000_000_000 * 100_000_000
 *   MAX_SCRIPT_SIZE                = 10_000
 * These aliases are not a second consensus lock.
 */
struct WalletSecurityLimits {
    static constexpr int64_t COIN = 100000000;
    static constexpr int64_t MAX_MONEY = static_cast<int64_t>(10000000000LL) * COIN;
    static constexpr uint32_t MAX_SCRIPT_SIZE = 10000;
    static constexpr uint32_t MAX_DESTINATION_SIZE = 10000;
    static constexpr uint32_t MAX_RPC_INPUT = 65536;
    static constexpr uint32_t MAX_ADDRESS_CHARS = 128;
    static constexpr uint32_t MAX_AMOUNT_CHARS = 32;
    static constexpr uint32_t MAX_IMPORT_BYTES = 512;
    static constexpr uint32_t MAX_INPUTS = 1000;
    static constexpr uint32_t MAX_OUTPUTS = 1000;
};

class TransactionBuilder
{
public:
    /**
     * Build a wallet-layer canonical transaction from an already-accepted
     * intent. Returns REJECT (empty canonical_tx) on any remaining check
     * failure. Does not serialize a consensus CTransaction and does not
     * call the Transaction Kernel.
     */
    static WalletSecurityResult Build(const WalletIntent& intent);
};

class WalletSecurityBoundary
{
public:
    /**
     * Fail-closed evaluation of one wallet request.
     * Never throws. Never mutates UTXO / ledger. Never calls consensus
     * formulas. Never logs secrets.
     */
    static WalletSecurityResult Evaluate(const WalletTransactionRequest& in);

    static WalletSecurityResult EvaluateRpc(const WalletRpcRequest& in);
    static WalletSecurityResult EvaluateImport(const WalletImportRequest& in);
    static WalletSecurityResult ParseAddress(const char* data, size_t size);
    static WalletSecurityResult ParseAmount(const char* data, size_t size);

    /** Named fuzz entry: decode bytes as a request and Evaluate. */
    static WalletSecurityResult EvaluateFuzz(const uint8_t* data, size_t size);

    /** Wipe secret bytes. Never logs. */
    static void WipeSecret(uint8_t* p, size_t n);

    /** True iff `s` looks like hex/WIF secret material (leak detector). */
    static bool LooksLikeSecret(const char* s);
};

#endif // BITCOIN_WALLET_SECURITY_WALLET_SECURITY_BOUNDARY_H
