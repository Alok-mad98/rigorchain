// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet signing boundary. No sighash rewrite. No Ed25519 rewrite.

#include <wallet/security/wallet_signing_boundary.h>
#include <wallet/security/wallet_security_boundary.h>

#include <cstring>

namespace {

uint64_t Fnv1a(const uint8_t* p, size_t n)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

void Mix32(const uint8_t* p, size_t n, uint8_t out32[32])
{
    for (int i = 0; i < 4; ++i) {
        uint8_t buf[8];
        uint64_t h = Fnv1a(p, n) ^ (0x9e3779b97f4a7c15ULL * static_cast<uint64_t>(i + 1));
        // Mix length so empty vs short payloads differ.
        h ^= static_cast<uint64_t>(n) + 1;
        h ^= Fnv1a(reinterpret_cast<const uint8_t*>(&i), sizeof(i));
        for (int b = 0; b < 8; ++b) buf[b] = static_cast<uint8_t>(h >> (8 * b));
        std::memcpy(out32 + i * 8, buf, 8);
    }
}

} // namespace

uint64_t WalletSigningBoundary::TxFingerprint(const uint8_t* tx, size_t n)
{
    return Fnv1a(tx ? tx : reinterpret_cast<const uint8_t*>(""), tx ? n : 0);
}

void WalletSigningBoundary::CanonicalBinding(const uint8_t* tx, size_t n, uint8_t out32[32])
{
    Mix32(tx ? tx : reinterpret_cast<const uint8_t*>(""), tx ? n : 0, out32);
}

WalletSecurityResult WalletSigningBoundary::Sign(WalletSigningRequest& req, WalletSignedRecord* out)
{
    WalletSecurityResult fail_closed = WalletSecurityResult::Reject("signing-fail-closed");
    try {
        if (!req.digest_layout_locked) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("sighash-layout-locked");
        }
        if (!req.witness_format_locked) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("witness-format-locked");
        }
        if (!req.signature_flags_locked) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("signature-flags-locked");
        }
        if (req.canonical_tx.empty()) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("empty-transaction");
        }
        if (!req.secret_set) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("wrong-key");
        }

        uint8_t expect[32];
        CanonicalBinding(req.canonical_tx.data(), req.canonical_tx.size(), expect);
        if (std::memcmp(expect, req.digest, 32) != 0) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("wrong-digest");
        }

        if (!out) {
            WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
            return WalletSecurityResult::Reject("signing-fail-closed");
        }

        WalletSignedRecord rec{};
        std::memcpy(rec.digest, req.digest, 32);
        rec.key_id = req.key_id;
        rec.tx_fingerprint = TxFingerprint(req.canonical_tx.data(), req.canonical_tx.size());
        rec.must_enter_transaction_kernel = true;
        // Isolation binding: mix key_id + digest. Not production Ed25519.
        // Production signing remains ed25519::SignDetached / CKey::Sign.
        for (int i = 0; i < 64; ++i) {
            const uint8_t d = req.digest[i % 32];
            const uint8_t k = req.secret[i];
            rec.signature_tag[i] = static_cast<uint8_t>(d ^ k ^ static_cast<uint8_t>(req.key_id >> ((i % 8) * 8)));
        }
        *out = rec;
        WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
        req.secret_set = false;
        auto r = WalletSecurityResult::Accept(WalletAdmissionTarget::SIGNING_API,
                                              "signed-to-kernel", /*enter_kernel=*/true);
        return r;
    } catch (...) {
        WalletSecurityBoundary::WipeSecret(req.secret, sizeof(req.secret));
        req.secret_set = false;
        return fail_closed;
    }
}

WalletSecurityResult WalletSigningBoundary::VerifyBinding(const WalletSignedRecord& rec,
                                                          const uint8_t* canonical_tx, size_t tx_len,
                                                          const uint8_t digest[32], uint64_t key_id)
{
    if (!canonical_tx || tx_len == 0) {
        return WalletSecurityResult::Reject("empty-transaction");
    }
    if (!digest) {
        return WalletSecurityResult::Reject("wrong-digest");
    }
    if (key_id != rec.key_id) {
        return WalletSecurityResult::Reject("wrong-key");
    }
    uint8_t expect[32];
    CanonicalBinding(canonical_tx, tx_len, expect);
    if (std::memcmp(expect, digest, 32) != 0 || std::memcmp(expect, rec.digest, 32) != 0) {
        return WalletSecurityResult::Reject("wrong-digest");
    }
    const uint64_t fp = TxFingerprint(canonical_tx, tx_len);
    if (fp != rec.tx_fingerprint) {
        return WalletSecurityResult::Reject("modified-tx-after-signing");
    }
    return WalletSecurityResult::Accept(WalletAdmissionTarget::VALIDATION_API,
                                        "binding-ok", /*enter_kernel=*/true);
}
