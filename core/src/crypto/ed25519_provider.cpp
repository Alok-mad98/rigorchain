#include <crypto/ed25519_provider.h>

#include <sodium.h>

namespace {
bool EnsureSodiumInit()
{
    return sodium_init() >= 0;
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

} // namespace ed25519
