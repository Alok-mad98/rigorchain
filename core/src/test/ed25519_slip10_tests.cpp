// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <key.h>
#include <key_io.h>
#include <pubkey.h>
#include <script/descriptor.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstring>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(ed25519_slip10_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_CASE(descriptor_rejects_unhardened_bip32)
{
    FlatSigningProvider keys;
    std::string error;
    CExtKey master;
    std::vector<std::byte> seed(32, std::byte{0x22});
    master.SetSeed(seed);
    const std::string xpub = EncodeExtPubKey(master.Neuter());
    const std::string xprv = EncodeExtKey(master);

    BOOST_CHECK(!Parse("wpkh(" + xpub + "/0/*)", keys, error));
    BOOST_CHECK(error.find("hardened") != std::string::npos);

    BOOST_CHECK(!Parse("wpkh(" + xpub + "/0/*')", keys, error));
    BOOST_CHECK(error.find("hardened") != std::string::npos);

    auto ok = Parse("wpkh(" + xprv + "/0'/*')", keys, error);
    BOOST_CHECK_MESSAGE(ok, error);
}

BOOST_AUTO_TEST_CASE(res_public_descriptor_omits_secret)
{
    std::array<unsigned char, ed25519::SEED_SIZE> seed{};
    seed.fill(0x33);
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pk{};
    std::array<unsigned char, ed25519::SECRETKEY_SIZE> sk{};
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk, sk));
    const std::string secret_hex = HexStr(Span<const unsigned char>{sk.data(), sk.size()});
    const std::string pk_hex = HexStr(Span<const unsigned char>{pk.data(), pk.size()});

    FlatSigningProvider keys;
    std::string error;
    auto desc = Parse("res(" + secret_hex + ")", keys, error);
    BOOST_REQUIRE_MESSAGE(desc, error);

    const std::string pub = desc->ToString();
    BOOST_CHECK(pub.find(secret_hex) == std::string::npos);
    BOOST_CHECK(pub.find(pk_hex) != std::string::npos);

    std::string priv;
    BOOST_CHECK(desc->ToPrivateString(keys, priv));
    BOOST_CHECK(priv.find(secret_hex) != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
