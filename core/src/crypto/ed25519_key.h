#ifndef BITCOIN_CRYPTO_ED25519_KEY_H
#define BITCOIN_CRYPTO_ED25519_KEY_H

#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <span.h>
#include <support/allocators/secure.h>
#include <uint256.h>

#include <array>
#include <vector>

class CPubKeyEd25519
{
private:
    std::array<unsigned char, ed25519::PUBKEY_SIZE> m_pubkey{};
    bool m_valid{false};

public:
    CPubKeyEd25519() = default;
    explicit CPubKeyEd25519(Span<const unsigned char> pubkey_bytes);

    bool Set(Span<const unsigned char> pubkey_bytes);
    bool IsValid() const { return m_valid; }

    const unsigned char* data() const { return m_pubkey.data(); }
    size_t size() const { return m_valid ? m_pubkey.size() : 0; }
    Span<const unsigned char> bytes() const { return Span<const unsigned char>{m_pubkey}.first(size()); }

    uint160 GetID() const;
    bool Verify(Span<const unsigned char> message, Span<const unsigned char> signature) const;
};

class CKeyEd25519
{
public:
    static constexpr size_t SEED_SIZE = ed25519::SEED_SIZE;
    static constexpr size_t SECRETKEY_SIZE = ed25519::SECRETKEY_SIZE;

private:
    std::array<unsigned char, ed25519::PUBKEY_SIZE> m_pubkey{};
    std::vector<unsigned char, secure_allocator<unsigned char>> m_secret_key;
    bool m_valid{false};

public:
    CKeyEd25519();

    bool IsValid() const { return m_valid; }
    void MakeNewKey();
    bool SetSeed(Span<const unsigned char> seed);

    CPubKeyEd25519 GetPubKey() const;
    bool Sign(Span<const unsigned char> message, std::array<unsigned char, ed25519::SIGNATURE_SIZE>& signature_out) const;
    Span<const unsigned char> GetSecretKey() const;
};

#endif // BITCOIN_CRYPTO_ED25519_KEY_H
