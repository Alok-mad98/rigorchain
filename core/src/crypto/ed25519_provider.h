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
/** SHA256(pk32) — consensus witness v1 RES program. */
bool ResWitnessProgram(Span<const unsigned char> pk32, Span<unsigned char> out32);
/** Initialize libsodium and refuse to run on an unsupported library version. */
bool EnsureRuntime();

} // namespace ed25519

#endif // BITCOIN_CRYPTO_ED25519_PROVIDER_H
