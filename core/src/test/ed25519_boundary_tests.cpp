// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/ed25519_provider.h>
#include <test/util/setup_common.h>
#include <script/predicate_verify.h>
#include <util/strencodings.h>

#include <sodium.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstring>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(ed25519_boundary_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(runtime_is_libsodium_ed25519_1_0_18_or_newer)
{
    BOOST_REQUIRE(ed25519::EnsureRuntime());
    BOOST_CHECK(sodium_library_version_major() > 10 ||
                (sodium_library_version_major() == 10 && sodium_library_version_minor() >= 3));
    BOOST_CHECK_EQUAL(std::string(crypto_sign_primitive()), std::string("ed25519"));
}

BOOST_AUTO_TEST_CASE(golden_acceptance_boundary)
{
    BOOST_REQUIRE(ed25519::EnsureRuntime());
    const unsigned char msg[32] = {0};

    unsigned char pk[32], sk[64], sig[64];
    BOOST_REQUIRE(crypto_sign_keypair(pk, sk) == 0);
    BOOST_REQUIRE(crypto_sign_detached(sig, nullptr, msg, sizeof(msg), sk) == 0);

    BOOST_CHECK(ed25519::VerifyDetached(Span{sig, 64}, Span{msg, 32}, Span{pk, 32}));

    static const unsigned char L_LE[32] = {
        0xed,0xd3,0xf5,0x5c,0x1a,0x63,0x12,0x58,0xd6,0x9c,0xf7,0xa2,0xde,0xf9,0xde,0x14,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10};
    unsigned char mutated[64];
    std::memcpy(mutated, sig, 64);
    unsigned int carry = 0;
    for (int i = 0; i < 32; i++) {
        carry += mutated[32 + i] + L_LE[i];
        mutated[32 + i] = static_cast<unsigned char>(carry & 0xff);
        carry >>= 8;
    }
    BOOST_CHECK(!ed25519::VerifyDetached(Span{mutated, 64}, Span{msg, 32}, Span{pk, 32}));

    static const unsigned char SMALL_ORDER[8][32] = {
        {0x01},
        {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
        {0x00},
        {0x00,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x80},
        {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
         0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05},
        {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
         0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a},
        {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
         0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x85},
        {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
         0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0xfa}};
    for (const auto& so : SMALL_ORDER) {
        BOOST_CHECK(!ed25519::VerifyDetached(Span{sig, 64}, Span{msg, 32}, Span{so, 32}));
    }

    unsigned char r0[64];
    std::memcpy(r0, sig, 64);
    std::memcpy(r0, SMALL_ORDER[0], 32);
    BOOST_CHECK(!ed25519::VerifyDetached(Span{r0, 64}, Span{msg, 32}, Span{pk, 32}));

    unsigned char zero[64]{};
    BOOST_CHECK(!ed25519::VerifyDetached(Span{zero, 64}, Span{msg, 32}, Span{SMALL_ORDER[0], 32}));

    unsigned char s1[64], s2[64];
    BOOST_REQUIRE(crypto_sign_detached(s1, nullptr, msg, sizeof(msg), sk) == 0);
    BOOST_REQUIRE(crypto_sign_detached(s2, nullptr, msg, sizeof(msg), sk) == 0);
    BOOST_CHECK(std::memcmp(s1, s2, 64) == 0);
}

BOOST_AUTO_TEST_CASE(rfc8032_official_vectors_and_length_boundaries)
{
    BOOST_REQUIRE(ed25519::EnsureRuntime());

    // RFC 8032 §7.1 TEST 1 (empty message).
    const auto seed1 = ParseHex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    const auto pk1 = ParseHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const auto sig1 = ParseHex(
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    std::vector<unsigned char> derived_pk(32), derived_sk(64);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed1, derived_pk, derived_sk));
    BOOST_CHECK(derived_pk == pk1);
    const std::vector<unsigned char> empty;
    BOOST_CHECK(ed25519::VerifyDetached(sig1, empty, pk1));

    // RFC 8032 §7.1 TEST 2 (message 0x72).
    const auto seed2 = ParseHex("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
    const auto pk2 = ParseHex("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
    const auto msg2 = ParseHex("72");
    const auto sig2 = ParseHex(
        "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
    std::vector<unsigned char> pk2d(32), sk2d(64);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed2, pk2d, sk2d));
    BOOST_CHECK(pk2d == pk2);
    BOOST_CHECK(ed25519::VerifyDetached(sig2, msg2, pk2));

    const unsigned char msg[32] = {0};
    unsigned char pk[32], sk[64], sig[64];
    BOOST_REQUIRE(crypto_sign_keypair(pk, sk) == 0);
    BOOST_REQUIRE(crypto_sign_detached(sig, nullptr, msg, sizeof(msg), sk) == 0);

    BOOST_CHECK(!ed25519::VerifyDetached(Span{sig, 63}, Span{msg, 32}, Span{pk, 32}));
    unsigned char sig65[65];
    std::memcpy(sig65, sig, 64);
    sig65[64] = 0x01;
    BOOST_CHECK(!ed25519::VerifyDetached(Span{sig65, 65}, Span{msg, 32}, Span{pk, 32}));
    BOOST_CHECK(!ed25519::VerifyDetached(Span<const unsigned char>(), Span{msg, 32}, Span{pk, 32}));
    BOOST_CHECK(!ed25519::VerifyDetached(Span{sig, 64}, Span{msg, 32}, Span{pk, 31}));
    unsigned char pk33[33];
    std::memcpy(pk33, pk, 32);
    pk33[32] = 0x00;
    BOOST_CHECK(!ed25519::VerifyDetached(Span{sig, 64}, Span{msg, 32}, Span{pk33, 33}));
}

BOOST_AUTO_TEST_CASE(o1_canonical_on_curve_pubkey)
{
    BOOST_REQUIRE(ed25519::EnsureRuntime());
    const auto pk1 = ParseHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    BOOST_CHECK(ed25519::IsCanonicalOnCurvePubkey(pk1));
    const std::vector<unsigned char> zeros(32, 0);
    BOOST_CHECK(!ed25519::IsCanonicalOnCurvePubkey(zeros));
    const std::vector<unsigned char> fill_ab(32, 0xab);
    BOOST_CHECK(!ed25519::IsCanonicalOnCurvePubkey(fill_ab));
    static const unsigned char SMALL_ORDER0[32] = {0x01};
    BOOST_CHECK(!ed25519::IsCanonicalOnCurvePubkey(Span{SMALL_ORDER0, 32}));
    // Classification is byte-shape only (TASK-010 R-2). Zeros are a well-formed
    // native RES program that is not a public key; policy/key_io refuse it.
    BOOST_CHECK(IsRigorResNativeWitnessProgram(pk1));
    BOOST_CHECK(IsRigorResNativeWitnessProgram(zeros));
    BOOST_CHECK(!IsRigorResNativeWitnessProgram(std::vector<unsigned char>(31, 0)));
}

BOOST_AUTO_TEST_SUITE_END()
