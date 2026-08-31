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

std::vector<unsigned char> CPubKey::GetEd25519Payload() const
{
    if (size() != COMPRESSED_SIZE || vch[0] != 0x00) return {};
    return {vch + 1, vch + COMPRESSED_SIZE};
}

uint160 CPubKey::GetEd25519KeyHash() const
{
    const std::vector<unsigned char> payload = GetEd25519Payload();
    if (payload.size() != ed25519::PUBKEY_SIZE) return GetID();
    return Hash160(payload);
}

uint256 CPubKey::GetResWitnessProgram() const
{
    uint256 out;
    const std::vector<unsigned char> payload = GetEd25519Payload();
    if (payload.size() != ed25519::PUBKEY_SIZE) return out;
    if (!ed25519::ResWitnessProgram(payload, Span<unsigned char>{out.begin(), 32})) return uint256{};
    return out;
}

bool CPubKey::Derive(CPubKey& pubkeyChild, ChainCode& ccChild, unsigned int nChild, const ChainCode& cc) const
{
    // C-1: public derivation of Ed25519 children leaks the child private seed.
    (void)pubkeyChild;
    (void)ccChild;
    (void)nChild;
    (void)cc;
    return false;
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
