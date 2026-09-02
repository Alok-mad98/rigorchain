// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet signing boundary.
//
// Production flow (unchanged by this module):
//   Transaction → Canonical sighash (existing sighash API) → Ed25519
//
// This boundary does not implement Ed25519, does not choose sighash layout,
// does not change witness format, and does not set signature flags.
// It binds a provided canonical digest to a signer and refuses:
//   wrong key, wrong digest, modified tx after signing, signature replay.
//
// Production signatures remain CKey::Sign / ed25519::SignDetached.

#ifndef BITCOIN_WALLET_SECURITY_WALLET_SIGNING_BOUNDARY_H
#define BITCOIN_WALLET_SECURITY_WALLET_SIGNING_BOUNDARY_H

#include <wallet/security/wallet_security_result.h>
#include <wallet/security/wallet_transaction_request.h>

#include <cstddef>
#include <cstdint>

struct WalletSignedRecord {
    uint8_t digest[32]{};
    uint64_t key_id{0};
    uint64_t tx_fingerprint{0};
    /** Isolation binding tag. Not a production Ed25519 signature. */
    uint8_t signature_tag[64]{};
    bool must_enter_transaction_kernel{true};
};

class WalletSigningBoundary
{
public:
    /**
     * Bind a canonical transaction to a signer.
     * Requires digest == CanonicalBinding(canonical_tx).
     * Requires digest_layout_locked / witness_format_locked /
     * signature_flags_locked. Wipes request.secret on the way out.
     * Never throws. Never logs the secret.
     */
    static WalletSecurityResult Sign(WalletSigningRequest& req, WalletSignedRecord* out);

    /** Reject if tx bytes, digest, or key_id no longer match the record. */
    static WalletSecurityResult VerifyBinding(const WalletSignedRecord& rec,
                                              const uint8_t* canonical_tx, size_t tx_len,
                                              const uint8_t digest[32], uint64_t key_id);

    /** Wallet-layer mutation binding. Not consensus sighash. */
    static void CanonicalBinding(const uint8_t* tx, size_t n, uint8_t out32[32]);
    static uint64_t TxFingerprint(const uint8_t* tx, size_t n);
};

#endif // BITCOIN_WALLET_SECURITY_WALLET_SIGNING_BOUNDARY_H
