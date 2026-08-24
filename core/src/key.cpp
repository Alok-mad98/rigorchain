// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>

#include <crypto/ed25519_provider.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <random.h>

#include <array>
#include <cstring>

namespace {
bool MakeEd25519KeypairFromSecret(const unsigned char* secret32, std::array<unsigned char, ed25519::PUBKEY_SIZE>& pub32, std::array<unsigned char, ed25519::SECRETKEY_SIZE>& sk64)
{
    return ed25519::SeedToKeypair(Span<const unsigned char>{secret32, ed25519::SEED_SIZE}, pub32, sk64);
}

CPubKey WrapEdPubToCPubKey(const std::array<unsigned char, ed25519::PUBKEY_SIZE>& pub32)
{
    unsigned char out[CPubKey::COMPRESSED_SIZE];
    out[0] = 0x00;
    std::memcpy(out + 1, pub32.data(), ed25519::PUBKEY_SIZE);
    return CPubKey(out, out + CPubKey::COMPRESSED_SIZE);
}
} // namespace

bool CKey::Check(const unsigned char* vch)
{
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pub32{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk64{};
    return MakeEd25519KeypairFromSecret(vch, pub32, sk64);
}

void CKey::MakeNewKey(bool fCompressedIn)
{
    do {
        GetStrongRandBytes(keydata);
    } while (!Check(keydata.data()));
    fValid = true;
    fCompressed = fCompressedIn;
}

bool CKey::Negate()
{
    // Ed25519 seeds are not secp256k1 scalars; bitwise NOT is not a group negation.
    (void)fValid;
    return false;
}

CPrivKey CKey::GetPrivKey() const
{
    assert(fValid);
    return CPrivKey(begin(), end());
}

CPubKey CKey::GetPubKey() const
{
    assert(fValid);
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pub32{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk64{};
    bool ok = MakeEd25519KeypairFromSecret(begin(), pub32, sk64);
    assert(ok);
    return WrapEdPubToCPubKey(pub32);
}

bool CKey::Sign(const uint256& hash, std::vector<unsigned char>& vchSig, bool, uint32_t) const
{
    if (!fValid) return false;
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pub32{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk64{};
    if (!MakeEd25519KeypairFromSecret(begin(), pub32, sk64)) return false;
    vchSig.assign(ed25519::SIGNATURE_SIZE, 0);
    return ed25519::SignDetached(Span<const unsigned char>{hash.begin(), 32},
        Span<const unsigned char>{sk64.data(), sk64.size()},
        Span<unsigned char>{vchSig.data(), vchSig.size()});
}

bool CKey::VerifyPubKey(const CPubKey& pubkey) const {
    if (pubkey.IsCompressed() != fCompressed) {
        return false;
    }
    unsigned char rnd[8];
    std::string str = "Bitcoin key verification\n";
    GetRandBytes(rnd);
    uint256 hash{Hash(str, rnd)};
    std::vector<unsigned char> vchSig;
    if (!Sign(hash, vchSig)) return false;
    return pubkey.Verify(hash, vchSig);
}

bool CKey::SignCompact(const uint256 &hash, std::vector<unsigned char>& vchSig) const {
    if (!fValid) return false;
    // Compact recoverable signatures are not supported under ED25519.
    (void)hash;
    vchSig.clear();
    return false;
}

bool CKey::Load(const CPrivKey &seckey, const CPubKey &vchPubKey, bool fSkipCheck=false) {
    if (seckey.size() != keydata.size()) return false;
    std::memcpy(keydata.data(), seckey.data(), keydata.size());
    fCompressed = true;
    fValid = Check(keydata.data());
    if (!fValid) return false;

    if (fSkipCheck)
        return true;

    return VerifyPubKey(vchPubKey);
}

bool CKey::Derive(CKey& keyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const {
    assert(IsValid());
    // SLIP-0010 / C-1: Ed25519 keys are hashed seeds, not secp256k1 scalars.
    // Additive BIP32 (child = I_L + parent mod n) does not exist, and using I_L as
    // the child seed makes that seed a pure function of the xpub. Fail closed.
    if ((nChild >> 31) == 0) return false;
    std::vector<unsigned char, secure_allocator<unsigned char>> vout(64);
    BIP32Hash(cc, nChild, 0, begin(), vout.data());
    std::memcpy(ccChild.begin(), vout.data() + 32, 32);
    keyChild.Set(vout.data(), vout.data() + 32, true);
    return keyChild.IsValid();
}

bool CExtKey::Derive(CExtKey &out, unsigned int _nChild) const {
    if (nDepth == std::numeric_limits<unsigned char>::max()) return false;
    out.nDepth = nDepth + 1;
    CKeyID id = key.GetPubKey().GetID();
    memcpy(out.vchFingerprint, &id, 4);
    out.nChild = _nChild;
    return key.Derive(out.key, out.chaincode, _nChild, chaincode);
}

void CExtKey::SetSeed(Span<const std::byte> seed)
{
    static const unsigned char hashkey[] = {'B','i','t','c','o','i','n',' ','s','e','e','d'};
    std::vector<unsigned char, secure_allocator<unsigned char>> vout(64);
    CHMAC_SHA512{hashkey, sizeof(hashkey)}.Write(UCharCast(seed.data()), seed.size()).Finalize(vout.data());
    key.Set(vout.data(), vout.data() + 32, true);
    memcpy(chaincode.begin(), vout.data() + 32, 32);
    nDepth = 0;
    nChild = 0;
    memset(vchFingerprint, 0, sizeof(vchFingerprint));
}

CExtPubKey CExtKey::Neuter() const {
    CExtPubKey ret;
    ret.nDepth = nDepth;
    memcpy(ret.vchFingerprint, vchFingerprint, 4);
    ret.nChild = nChild;
    ret.pubkey = key.GetPubKey();
    ret.chaincode = chaincode;
    return ret;
}

void CExtKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const {
    code[0] = nDepth;
    memcpy(code+1, vchFingerprint, 4);
    WriteBE32(code+5, nChild);
    memcpy(code+9, chaincode.begin(), 32);
    code[41] = 0;
    assert(key.size() == 32);
    memcpy(code+42, key.begin(), 32);
}

void CExtKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code+1, 4);
    nChild = ReadBE32(code+5);
    memcpy(chaincode.begin(), code+9, 32);
    key.Set(code+42, code+BIP32_EXTKEY_SIZE, true);
    if ((nDepth == 0 && (nChild != 0 || ReadLE32(vchFingerprint) != 0)) || code[41] != 0) key = CKey();
}

bool ECC_InitSanityCheck() {
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    return key.VerifyPubKey(pubkey);
}

void ECC_Start() {
    // Pins libsodium and the Ed25519 primitive. Failure is fatal: a node that
    // cannot initialize the consensus verifier must not produce or check blocks.
    if (!ed25519::EnsureRuntime()) {
        throw std::runtime_error("libsodium Ed25519 runtime is missing or too old (need >= 1.0.18)");
    }
}

void ECC_Stop() {
    // No global context needed for ED25519 provider.
}
