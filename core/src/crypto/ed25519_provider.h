#ifndef BITCOIN_CRYPTO_ED25519_PROVIDER_H
#define BITCOIN_CRYPTO_ED25519_PROVIDER_H

#include <span.h>

#include <cstddef>

namespace ed25519 {

static constexpr size_t PUBKEY_SIZE = 32;
static constexpr size_t SECRETKEY_SIZE = 64;
static constexpr size_t SEED_SIZE = 32;
static constexpr size_t SIGNATURE_SIZE = 64;

bool VerifyDetached(Span<const unsigned char> signature, Span<const unsigned char> message, Span<const unsigned char> pubkey);
bool SignDetached(Span<const unsigned char> message, Span<const unsigned char> secret_key, Span<unsigned char> signature_out);
bool SeedToKeypair(Span<const unsigned char> seed, Span<unsigned char> pubkey_out, Span<unsigned char> secret_key_out);
/** Option B: witness v1 RES program is the 32-byte Ed25519 public key itself. */
bool ResWitnessProgram(Span<const unsigned char> pk32, Span<unsigned char> out32);
/**
 * True iff pk32 is a 32-byte canonical encoding of a point on the edwards25519
 * prime-order subgroup (libsodium `crypto_core_ed25519_is_valid_point`).
 * Rejects identity, small-order, and non-canonical encodings.
 *
 * address encode/decode, relay policy, and wallet destination
 * construction. Not a consensus classifier. `IsRigorResNativeWitnessProgram`
 * stays a pure byte predicate so a confirmed UTXO cannot change class when
 * libsodium's notion of a valid point changes.
 */
bool IsCanonicalOnCurvePubkey(Span<const unsigned char> pk32);
/** Initialize libsodium and refuse to run on an unsupported library version. */
bool EnsureRuntime();

} // namespace ed25519

#endif // BITCOIN_CRYPTO_ED25519_PROVIDER_H
