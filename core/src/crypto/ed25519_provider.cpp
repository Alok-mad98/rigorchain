#include <crypto/ed25519_provider.h>

#include <sodium.h>

#include <cstring>

namespace {
bool EnsureSodiumInit()
{
    if (sodium_init() < 0) return false;
    // H-1: the Ed25519 acceptance boundary is a consensus rule. Refuse libraries
    // older than 1.0.18 (SODIUM_LIBRARY_VERSION 10.3) and non-ed25519 primitives.
#if SODIUM_LIBRARY_VERSION_MAJOR < 10 || (SODIUM_LIBRARY_VERSION_MAJOR == 10 && SODIUM_LIBRARY_VERSION_MINOR < 3)
#error "RIGOR requires libsodium >= 1.0.18"
#endif
    if (sodium_library_version_major() < 10) return false;
    if (sodium_library_version_major() == 10 && sodium_library_version_minor() < 3) return false;
    if (std::strcmp(crypto_sign_primitive(), "ed25519") != 0) return false;
    return true;
}
} // namespace

namespace ed25519 {

bool VerifyDetached(Span<const unsigned char> signature, Span<const unsigned char> message, Span<const unsigned char> pubkey)
{
    if (!EnsureSodiumInit()) return false;
    if (signature.size() != SIGNATURE_SIZE || pubkey.size() != PUBKEY_SIZE) return false;
    return crypto_sign_verify_detached(signature.data(), message.data(), message.size(), pubkey.data()) == 0;
}

bool SignDetached(Span<const unsigned char> message, Span<const unsigned char> secret_key, Span<unsigned char> signature_out)
{
    if (!EnsureSodiumInit()) return false;
    if (secret_key.size() != SECRETKEY_SIZE || signature_out.size() != SIGNATURE_SIZE) return false;
    return crypto_sign_detached(signature_out.data(), nullptr, message.data(), message.size(), secret_key.data()) == 0;
}

bool SeedToKeypair(Span<const unsigned char> seed, Span<unsigned char> pubkey_out, Span<unsigned char> secret_key_out)
{
    if (!EnsureSodiumInit()) return false;
    if (seed.size() != SEED_SIZE || pubkey_out.size() != PUBKEY_SIZE || secret_key_out.size() != SECRETKEY_SIZE) return false;
    return crypto_sign_seed_keypair(pubkey_out.data(), secret_key_out.data(), seed.data()) == 0;
}

bool ResWitnessProgram(Span<const unsigned char> pk32, Span<unsigned char> out32)
{
    if (pk32.size() != PUBKEY_SIZE || out32.size() != 32) return false;
    std::memcpy(out32.data(), pk32.data(), PUBKEY_SIZE);
    return true;
}

bool IsCanonicalOnCurvePubkey(Span<const unsigned char> pk32)
{
    if (!EnsureSodiumInit()) return false;
    if (pk32.size() != PUBKEY_SIZE) return false;
    return crypto_core_ed25519_is_valid_point(pk32.data()) == 1;
}

bool EnsureRuntime()
{
    return EnsureSodiumInit();
}

} // namespace ed25519
