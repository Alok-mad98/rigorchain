// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/checks.h>

#include <crypto/ed25519_provider.h>
#include <key.h>
#include <random.h>
#include <util/time.h>
#include <util/translation.h>

#include <memory>

namespace kernel {

std::optional<bilingual_str> SanityChecks(const Context&)
{
    if (!ECC_InitSanityCheck()) {
        return Untranslated("Elliptic curve cryptography sanity check failure. Aborting.");
    }

    // TASK-010 R-3: Ed25519 is the RES ownership boundary, so an unusable libsodium
    // is not a degraded mode. Every `ed25519::` entry point fails closed by
    // returning false, which at consensus scope would silently reject valid RES
    // spends and follow a different chain. Refuse to start instead.
    if (!ed25519::EnsureRuntime()) {
        return Untranslated("Ed25519 (libsodium) sanity check failure: RES consensus requires libsodium >= 1.0.18 with the ed25519 primitive. Aborting.");
    }

    if (!Random_SanityCheck()) {
        return Untranslated("OS cryptographic RES sanity check failure. Aborting.");
    }

    if (!ChronoSanityCheck()) {
        return Untranslated("Clock epoch mismatch. Aborting.");
    }

    return std::nullopt;
}

}
