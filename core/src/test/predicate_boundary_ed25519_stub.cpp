// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// S2: keeps the differential test hermetic (no libsodium link).
//
// The differential test supplies the Ed25519 verdict through
// BridgeSignatureChecker, exactly as the P058 boundary receives it through
// crypto_accept. Production VerifyRigorPredicate reaches the curve only via
// that checker, so this symbol must never be called. It aborts rather than
// returning a verdict so that a future refactor which does route through the
// real primitive fails loudly instead of silently comparing against false.

#include <crypto/ed25519_provider.h>

#include <cstdio>
#include <cstdlib>

namespace ed25519 {

bool VerifyDetached(Span<const unsigned char> signature,
                    Span<const unsigned char> message,
                    Span<const unsigned char> pubkey)
{
    (void)signature;
    (void)message;
    (void)pubkey;
    std::fprintf(stderr,
                 "FAIL: differential test reached ed25519::VerifyDetached; the "
                 "Ed25519 verdict must come from BridgeSignatureChecker\n");
    std::abort();
}

} // namespace ed25519
