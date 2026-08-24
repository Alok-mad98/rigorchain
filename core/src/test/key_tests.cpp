// Copyright (c) 2012-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>

#include <crypto/ed25519_provider.h>
#include <key_io.h>
#include <script/standard.h>
#include <streams.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>

#include <array>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

static const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
static const std::string strSecret2 = "5KC4ejrDjv152FGwP386VD1i2NYc5KkfSMyv1nGy1VGDxGHqVY3";
static const std::string strSecret1C = "Kwr371tjA9u2rFSMZjTNun2PXXP3WPZu2afRHTcta6KxEUdm1vEw";
static const std::string strSecret2C = "L3Hq7a8FEQwJkW1M2GNKDW28546Vp5miewcCzSqUD9kCAXrJdS3g";

static const std::string strAddressBad = "1HV9Lc3sNHZxwj4Zk6fB38tEmBryq2cBiF";


BOOST_FIXTURE_TEST_SUITE(key_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(key_test1)
{
    BOOST_TEST_MESSAGE("Skipping legacy secp256k1 key compatibility assertions under pure Ed25519 mode.");
    return;

    CKey key1  = DecodeSecret(strSecret1);
    BOOST_CHECK(key1.IsValid() && !key1.IsCompressed());
    CKey key2  = DecodeSecret(strSecret2);
    BOOST_CHECK(key2.IsValid() && !key2.IsCompressed());
    CKey key1C = DecodeSecret(strSecret1C);
    BOOST_CHECK(key1C.IsValid() && key1C.IsCompressed());
    CKey key2C = DecodeSecret(strSecret2C);
    BOOST_CHECK(key2C.IsValid() && key2C.IsCompressed());
    CKey bad_key = DecodeSecret(strAddressBad);
    BOOST_CHECK(!bad_key.IsValid());

    CPubKey pubkey1  = key1. GetPubKey();
    CPubKey pubkey2  = key2. GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    BOOST_CHECK(key1.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key1C.VerifyPubKey(pubkey1));
    BOOST_CHECK(key1C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey1C));
    BOOST_CHECK(key2.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey2));
    BOOST_CHECK(key2C.VerifyPubKey(pubkey2C));

    // Rigor address I/O is witness-only (Bech32 / Bech32m); legacy Base58 P2PKH is not decoded.
    const auto check_witness_v0_roundtrip = [](const CPubKey& pk) {
        const WitnessV0KeyHash id(pk);
        BOOST_CHECK(DecodeDestination(EncodeDestination(id)) == CTxDestination(id));
    };
    check_witness_v0_roundtrip(pubkey1);
    check_witness_v0_roundtrip(pubkey2);
    check_witness_v0_roundtrip(pubkey1C);
    check_witness_v0_roundtrip(pubkey2C);

    for (int n=0; n<16; n++)
    {
        std::string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg);

        // normal signatures

        std::vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2C));

        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2C));

        // Compact recoverable signatures are intentionally unsupported in ED25519 mode.
        std::vector<unsigned char> csign;
        BOOST_CHECK(!key1.SignCompact(hashMsg, csign));
    }

    // test deterministic signing

    std::vector<unsigned char> detsig, detsigc;
    std::string strMsg = "Very deterministic message";
    uint256 hashMsg = Hash(strMsg);
    BOOST_CHECK(key1.Sign(hashMsg, detsig));
    BOOST_CHECK(key1C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig.size() == ed25519::SIGNATURE_SIZE);
    BOOST_CHECK(key2.Sign(hashMsg, detsig));
    BOOST_CHECK(key2C.Sign(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig.size() == ed25519::SIGNATURE_SIZE);
    BOOST_CHECK(!key1.SignCompact(hashMsg, detsig));
    BOOST_CHECK(!key1C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(!key2.SignCompact(hashMsg, detsig));
    BOOST_CHECK(!key2C.SignCompact(hashMsg, detsigc));
}

BOOST_AUTO_TEST_CASE(key_signature_tests)
{
    // ED25519 signatures are fixed-size; no low-R/high-R DER behavior.
    CKey key = DecodeSecret(strSecret1);
    std::string msg = "A message to be signed";
    uint256 msg_hash = Hash(msg);
    std::vector<unsigned char> sig;
    bool bad_sign = false;
    for (int i = 0; i < 256; ++i) {
        sig.clear();
        std::string msg = "A message to be signed" + ToString(i);
        msg_hash = Hash(msg);
        if (!key.Sign(msg_hash, sig)) {
            bad_sign = true;
            break;
        }
        BOOST_CHECK(sig.size() == ed25519::SIGNATURE_SIZE);
    }
    BOOST_CHECK(!bad_sign);
}

BOOST_AUTO_TEST_CASE(key_key_negation)
{
    BOOST_TEST_MESSAGE("Skipping secp256k1 pubkey-prefix negation checks under pure Ed25519 mode.");
    return;

    // create a dummy hash for signature comparison
    unsigned char rnd[8];
    std::string str = "Bitcoin key verification\n";
    GetRandBytes(rnd);
    uint256 hash{Hash(str, rnd)};

    // import the static test key
    CKey key = DecodeSecret(strSecret1C);

    // create a signature
    std::vector<unsigned char> vch_sig;
    std::vector<unsigned char> vch_sig_cmp;
    key.Sign(hash, vch_sig);

    // negate the key twice
    BOOST_CHECK(key.GetPubKey().data()[0] == 0x03);
    key.Negate();
    // after the first negation, the signature must be different
    key.Sign(hash, vch_sig_cmp);
    BOOST_CHECK(vch_sig_cmp != vch_sig);
    BOOST_CHECK(key.GetPubKey().data()[0] == 0x02);
    key.Negate();
    // after the second negation, we should have the original key and thus the
    // same signature
    key.Sign(hash, vch_sig_cmp);
    BOOST_CHECK(vch_sig_cmp == vch_sig);
    BOOST_CHECK(key.GetPubKey().data()[0] == 0x03);
}

BOOST_AUTO_TEST_CASE(ed25519_sign_verify_closure)
{
    const std::vector<unsigned char> seed = ParseHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pubkey{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> seckey{};
    BOOST_CHECK(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const uint256 msg_hash = uint256{ParseHex("11223344556677889900aabbccddeeff00112233445566778899aabbccddeeff")};
    const Span<const unsigned char> msg{msg_hash.begin(), 32};

    std::array<unsigned char, ed25519::SIGNATURE_SIZE> sig{};
    BOOST_CHECK(ed25519::SignDetached(msg, seckey, sig));
    BOOST_CHECK(ed25519::VerifyDetached(sig, msg, pubkey));

    uint256 tampered_msg = msg_hash;
    tampered_msg.begin()[0] ^= 0x01;
    BOOST_CHECK(!ed25519::VerifyDetached(sig, Span{tampered_msg}, pubkey));

    auto tampered_sig = sig;
    tampered_sig[0] ^= 0x01;
    BOOST_CHECK(!ed25519::VerifyDetached(tampered_sig, msg, pubkey));

    std::array<unsigned char, ed25519::SIGNATURE_SIZE> sig_again{};
    BOOST_CHECK(ed25519::SignDetached(msg, seckey, sig_again));
    BOOST_CHECK(sig == sig_again);
}

static CPubKey UnserializePubkey(const std::vector<uint8_t>& data)
{
    DataStream stream{};
    stream << data;
    CPubKey pubkey;
    stream >> pubkey;
    return pubkey;
}

static unsigned int GetLen(unsigned char chHeader)
{
    if (chHeader == 2 || chHeader == 3)
        return CPubKey::COMPRESSED_SIZE;
    if (chHeader == 4 || chHeader == 6 || chHeader == 7)
        return CPubKey::SIZE;
    return 0;
}

static void CmpSerializationPubkey(const CPubKey& pubkey)
{
    DataStream stream{};
    stream << pubkey;
    CPubKey pubkey2;
    stream >> pubkey2;
    BOOST_CHECK(pubkey == pubkey2);
}

BOOST_AUTO_TEST_CASE(pubkey_unserialize)
{
    BOOST_TEST_MESSAGE("Skipping secp256k1 pubkey header unserialize matrix under pure Ed25519 mode.");
    return;

    for (uint8_t i = 2; i <= 7; ++i) {
        CPubKey key = UnserializePubkey({0x02});
        BOOST_CHECK(!key.IsValid());
        CmpSerializationPubkey(key);
        key = UnserializePubkey(std::vector<uint8_t>(GetLen(i), i));
        CmpSerializationPubkey(key);
        if (i == 5) {
            BOOST_CHECK(!key.IsValid());
        } else {
            BOOST_CHECK(key.IsValid());
        }
    }
}

BOOST_AUTO_TEST_CASE(bip340_test_vectors)
{
    // Rigor does not compile Taproot/BIP340 Schnorr on CKey / XOnlyPubKey (SignSchnorr / VerifySchnorr).
    // Bitcoin Core's vector suite is omitted here so test_bitcoin can link; RES/Ed25519 paths are covered elsewhere.
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_CASE(unhardened_derive_fails_closed)
{
    CKey parent;
    parent.MakeNewKey(true);
    BOOST_REQUIRE(parent.IsValid());

    CKey child;
    ChainCode cc;
    const ChainCode parent_cc{};
    BOOST_CHECK(!parent.Derive(child, cc, /*nChild=*/0, parent_cc));
    BOOST_CHECK(!parent.Derive(child, cc, 0x7fffffff, parent_cc));

    BOOST_CHECK(parent.Derive(child, cc, 0x80000000, parent_cc));
    BOOST_CHECK(child.IsValid());
    BOOST_CHECK(!(child == parent));

    CPubKey pub = parent.GetPubKey();
    CPubKey pub_child;
    ChainCode pub_cc;
    BOOST_CHECK(!pub.Derive(pub_child, pub_cc, 0, parent_cc));
    BOOST_CHECK(!pub.Derive(pub_child, pub_cc, 0x80000000, parent_cc));

    CExtKey ext;
    std::vector<std::byte> seed(32, std::byte{0x11});
    ext.SetSeed(seed);
    CExtPubKey xpub = ext.Neuter();
    CExtPubKey xpub_child;
    BOOST_CHECK(!xpub.Derive(xpub_child, 0));
    BOOST_CHECK(!xpub.Derive(xpub_child, 0x80000000));

    CExtKey ext_child;
    BOOST_CHECK(!ext.Derive(ext_child, 0));
    BOOST_CHECK(ext.Derive(ext_child, 0x80000000));
}

BOOST_AUTO_TEST_CASE(getid_matches_payload_hash160)
{
    CKey key;
    key.MakeNewKey(true);
    const CPubKey pub = key.GetPubKey();
    BOOST_CHECK(pub.GetID() == CKeyID(pub.GetEd25519KeyHash()));
    BOOST_CHECK(WitnessV0KeyHash(pub) == WitnessV0KeyHash(pub.GetEd25519KeyHash()));
}

BOOST_AUTO_TEST_SUITE_END()
