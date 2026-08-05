#include <crypto/ed25519_key.h>

#include <random.h>

#include <algorithm>

CPubKeyEd25519::CPubKeyEd25519(Span<const unsigned char> pubkey_bytes)
{
    Set(pubkey_bytes);
}

bool CPubKeyEd25519::Set(Span<const unsigned char> pubkey_bytes)
{
    if (pubkey_bytes.size() != ed25519::PUBKEY_SIZE) {
        m_valid = false;
        return false;
    }
    std::copy(pubkey_bytes.begin(), pubkey_bytes.end(), m_pubkey.begin());
    m_valid = true;
    return true;
}

uint160 CPubKeyEd25519::GetID() const
{
    if (!m_valid) return uint160{};
    return Hash160(m_pubkey);
}

bool CPubKeyEd25519::Verify(Span<const unsigned char> message, Span<const unsigned char> signature) const
{
    if (!m_valid) return false;
    return ed25519::VerifyDetached(signature, message, m_pubkey);
}

CKeyEd25519::CKeyEd25519()
{
    m_secret_key.resize(ed25519::SECRETKEY_SIZE);
}

void CKeyEd25519::MakeNewKey()
{
    std::array<unsigned char, ed25519::SEED_SIZE> seed{};
    GetStrongRandBytes(seed);
    m_valid = SetSeed(seed);
}

bool CKeyEd25519::SetSeed(Span<const unsigned char> seed)
{
    m_valid = ed25519::SeedToKeypair(seed,
        Span<unsigned char>{m_pubkey.data(), m_pubkey.size()},
        Span<unsigned char>{m_secret_key.data(), m_secret_key.size()});
    return m_valid;
}

CPubKeyEd25519 CKeyEd25519::GetPubKey() const
{
    if (!m_valid) return {};
    return CPubKeyEd25519{Span<const unsigned char>{m_pubkey.data(), m_pubkey.size()}};
}

bool CKeyEd25519::Sign(Span<const unsigned char> message, std::array<unsigned char, ed25519::SIGNATURE_SIZE>& signature_out) const
{
    if (!m_valid) return false;
    return ed25519::SignDetached(message,
        Span<const unsigned char>{m_secret_key.data(), m_secret_key.size()},
        Span<unsigned char>{signature_out.data(), signature_out.size()});
}

Span<const unsigned char> CKeyEd25519::GetSecretKey() const
{
    if (!m_valid) return {};
    return Span<const unsigned char>{m_secret_key.data(), m_secret_key.size()};
}
