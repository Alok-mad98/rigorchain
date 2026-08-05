// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pubkey.h>

#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <span.h>
#include <uint256.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

XOnlyPubKey::XOnlyPubKey(Span<const unsigned char> bytes)
{
    assert(bytes.size() == 32);
    std::copy(bytes.begin(), bytes.end(), m_keydata.begin());
}

std::vector<CKeyID> XOnlyPubKey::GetKeyIDs() const
{
    std::vector<CKeyID> out;
    unsigned char b[33] = {0x00};
    std::copy(m_keydata.begin(), m_keydata.end(), b + 1);
    CPubKey fullpubkey;
    fullpubkey.Set(b, b + 33);
    out.push_back(fullpubkey.GetID());
    return out;
}

bool XOnlyPubKey::IsFullyValid() const
{
    return !IsNull();
}

static const HashWriter HASHER_TAPTWEAK{TaggedHash("TapTweak")};

uint256 XOnlyPubKey::ComputeTapTweakHash(const uint256* merkle_root) const
{
    if (merkle_root == nullptr) {
        return (HashWriter{HASHER_TAPTWEAK} << m_keydata).GetSHA256();
    } else {
        return (HashWriter{HASHER_TAPTWEAK} << m_keydata << *merkle_root).GetSHA256();
    }
}

bool XOnlyPubKey::CheckTapTweak(const XOnlyPubKey& internal, const uint256& merkle_root, bool parity) const
{
    auto tweaked = internal.CreateTapTweak(&merkle_root);
    return tweaked.has_value() && tweaked->first == *this && tweaked->second == parity;
}

std::optional<std::pair<XOnlyPubKey, bool>> XOnlyPubKey::CreateTapTweak(const uint256* merkle_root) const
{
    if (IsNull()) return std::nullopt;
    const uint256 tweak = ComputeTapTweakHash(merkle_root);
    std::pair<XOnlyPubKey, bool> ret;
    for (size_t i = 0; i < XOnlyPubKey::size(); ++i) {
        ret.first.begin()[i] = m_keydata.begin()[i] ^ tweak.begin()[i];
    }
    ret.second = false;
    return ret;
}

bool CPubKey::Verify(const uint256& hash, const std::vector<unsigned char>& vchSig) const
{
    if (!IsValid()) return false;
    if (size() != COMPRESSED_SIZE) return false;
    if (vch[0] != 0x00) return false;
    if (vchSig.size() != ed25519::SIGNATURE_SIZE) return false;
    return ed25519::VerifyDetached(Span<const unsigned char>{vchSig.data(), vchSig.size()},
        Span<const unsigned char>{hash.begin(), 32},
        Span<const unsigned char>{vch + 1, ed25519::PUBKEY_SIZE});
}

bool CPubKey::RecoverCompact(const uint256& hash, const std::vector<unsigned char>& vchSig)
{
    (void)hash;
    (void)vchSig;
    return false;
}

bool CPubKey::IsFullyValid() const
{
    return IsValid();
}

bool CPubKey::Decompress()
{
    return IsValid();
}

bool CPubKey::Derive(CPubKey& pubkeyChild, ChainCode& ccChild, unsigned int nChild, const ChainCode& cc) const
{
    assert(IsValid());
    unsigned char out[64];
    BIP32Hash(cc, nChild, *begin(), begin() + 1, out);
    std::memcpy(ccChild.begin(), out + 32, 32);
    if (*begin() == 0x00) {
        // ORUM: I_L is an Ed25519 seed; wire pubkey must match CKey::Derive + GetPubKey(), not raw I_L.
        std::array<unsigned char, ed25519::PUBKEY_SIZE> pub32{};
        std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk64{};
        if (!ed25519::SeedToKeypair({out, ed25519::SEED_SIZE}, pub32, sk64)) return false;
        unsigned char pub[COMPRESSED_SIZE];
        pub[0] = 0x00;
        std::memcpy(pub + 1, pub32.data(), ed25519::PUBKEY_SIZE);
        pubkeyChild.Set(pub, pub + COMPRESSED_SIZE);
        return true;
    }
    unsigned char pub[COMPRESSED_SIZE];
    pub[0] = 0x00;
    std::memcpy(pub + 1, out, ed25519::PUBKEY_SIZE);
    pubkeyChild.Set(pub, pub + COMPRESSED_SIZE);
    return true;
}

void CExtPubKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const
{
    code[0] = nDepth;
    std::memcpy(code + 1, vchFingerprint, 4);
    WriteBE32(code + 5, nChild);
    std::memcpy(code + 9, chaincode.begin(), 32);
    assert(pubkey.size() == CPubKey::COMPRESSED_SIZE);
    std::memcpy(code + 41, pubkey.begin(), CPubKey::COMPRESSED_SIZE);
}

void CExtPubKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE])
{
    nDepth = code[0];
    std::memcpy(vchFingerprint, code + 1, 4);
    nChild = ReadBE32(code + 5);
    std::memcpy(chaincode.begin(), code + 9, 32);
    pubkey.Set(code + 41, code + BIP32_EXTKEY_SIZE);
    if ((nDepth == 0 && (nChild != 0 || ReadLE32(vchFingerprint) != 0)) || !pubkey.IsFullyValid()) pubkey = CPubKey();
}

void CExtPubKey::EncodeWithVersion(unsigned char code[BIP32_EXTKEY_WITH_VERSION_SIZE]) const
{
    std::memcpy(code, version, 4);
    Encode(&code[4]);
}

void CExtPubKey::DecodeWithVersion(const unsigned char code[BIP32_EXTKEY_WITH_VERSION_SIZE])
{
    std::memcpy(version, code, 4);
    Decode(&code[4]);
}

bool CExtPubKey::Derive(CExtPubKey& out, unsigned int _nChild) const
{
    if (nDepth == std::numeric_limits<unsigned char>::max()) return false;
    out.nDepth = nDepth + 1;
    CKeyID id = pubkey.GetID();
    std::memcpy(out.vchFingerprint, &id, 4);
    out.nChild = _nChild;
    return pubkey.Derive(out.pubkey, out.chaincode, _nChild, chaincode);
}

/* static */ bool CPubKey::CheckLowS(const std::vector<unsigned char>& vchSig)
{
    return vchSig.size() == ed25519::SIGNATURE_SIZE;
}

bool CPubKey::NormalizeSignatureDER_LowS(std::vector<unsigned char>& vchSigDer)
{
    return vchSigDer.size() == ed25519::SIGNATURE_SIZE;
}
