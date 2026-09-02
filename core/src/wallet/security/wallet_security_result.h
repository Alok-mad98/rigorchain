// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet security-boundary output.
// Wallet may emit only ACCEPT_TO_BUILDER or REJECT.
// It must not mutate UTXO / ledger or enter ConsensusKernel.

#ifndef BITCOIN_WALLET_SECURITY_WALLET_SECURITY_RESULT_H
#define BITCOIN_WALLET_SECURITY_WALLET_SECURITY_RESULT_H

#include <cstdint>
#include <vector>

/** Exclusive wallet-layer decisions. Do not add a consensus-accept. */
enum class WalletSecurityDecision : uint8_t {
    ACCEPT_TO_BUILDER = 0,
    REJECT = 1,
};

/** Where ACCEPT_TO_BUILDER is allowed to go next. Never ConsensusKernel. */
enum class WalletAdmissionTarget : uint8_t {
    NONE = 0,
    TRANSACTION_BUILDER = 1,
    SIGNING_API = 2,
    VALIDATION_API = 3, // request VerifyRigorPredicate; do not implement it
    IMPORT = 4,
};

struct WalletSecurityResult {
    WalletSecurityDecision decision{WalletSecurityDecision::REJECT};
    WalletAdmissionTarget admission{WalletAdmissionTarget::NONE};
    const char* reason{"uninitialized"};
    /** Structural accept still must enter Transaction Kernel. */
    bool must_enter_transaction_kernel{false};
    /** True iff this result must not contain secret material. */
    bool secret_isolated{true};
    std::vector<uint8_t> canonical_tx;

    static WalletSecurityResult Accept(WalletAdmissionTarget target, const char* why,
                                       bool enter_kernel = true)
    {
        WalletSecurityResult r;
        r.decision = WalletSecurityDecision::ACCEPT_TO_BUILDER;
        r.admission = target;
        r.reason = why;
        r.must_enter_transaction_kernel = enter_kernel;
        r.secret_isolated = true;
        return r;
    }

    static WalletSecurityResult Reject(const char* why)
    {
        WalletSecurityResult r;
        r.decision = WalletSecurityDecision::REJECT;
        r.admission = WalletAdmissionTarget::NONE;
        r.reason = why;
        r.must_enter_transaction_kernel = false;
        r.secret_isolated = true;
        return r;
    }

    bool IsAccept() const { return decision == WalletSecurityDecision::ACCEPT_TO_BUILDER; }
    bool IsReject() const { return decision == WalletSecurityDecision::REJECT; }
};

#endif // BITCOIN_WALLET_SECURITY_WALLET_SECURITY_RESULT_H
