// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/common.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <seed_manifest.h>
#include <span.h>
#include <test/util/setup_common.h>
#include <tinyformat.h>
#include <util/strencodings.h>

#include <array>
#include <string>
#include <string_view>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(seed_manifest_tests, BasicTestingSetup)

static void TestKeypair(std::array<unsigned char, 32>& pub, std::array<unsigned char, 64>& sec)
{
    const std::string label{"RIGOR_SEED_MANIFEST_TEST"};
    std::array<unsigned char, 32> seed{};
    CSHA256().Write(UCharCast(label.data()), label.size()).Finalize(seed.data());
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pub, sec));
}

static SeedManifest SignedManifest(const std::array<unsigned char, 64>& sec, std::string_view chain)
{
    SeedManifest m;
    m.epoch = "2026Q3";
    m.expiry = 1798761600;
    m.nodes = {"203.0.113.11:38732", "203.0.113.10:38732"};
    const std::string msg = SeedManifestSignedMessage(m, chain);
    m.signature.assign(ed25519::SIGNATURE_SIZE, 0);
    BOOST_REQUIRE(ed25519::SignDetached(MakeUCharSpan(msg), sec, m.signature));
    return m;
}

BOOST_AUTO_TEST_CASE(canonical_message_is_injective)
{
    std::array<unsigned char, 32> pub{};
    std::array<unsigned char, 64> sec{};
    TestKeypair(pub, sec);

    SeedManifest a;
    a.epoch = "2026Q3";
    a.expiry = 1798761600;
    a.nodes = {"b", "c"};
    SeedManifest b = a;
    b.nodes = {"b\nc"};
    BOOST_CHECK(SeedManifestSignedMessage(a, "regtest") != SeedManifestSignedMessage(b, "regtest"));

    SeedManifest c = a;
    c.epoch = "2026Q3\nexpiry=1";
    BOOST_CHECK(SeedManifestSignedMessage(a, "regtest") != SeedManifestSignedMessage(c, "regtest"));

    BOOST_CHECK(SeedManifestSignedMessage(a, "main") != SeedManifestSignedMessage(a, "test"));
}

BOOST_AUTO_TEST_CASE(canonical_message_and_signature)
{
    std::array<unsigned char, 32> pub{};
    std::array<unsigned char, 64> sec{};
    TestKeypair(pub, sec);

    SeedManifest m = SignedManifest(sec, "regtest");
    BOOST_CHECK(VerifySeedManifestSignature(m, pub, "regtest"));
    BOOST_CHECK(!VerifySeedManifestSignature(m, pub, "main"));

    m.signature[0] ^= 0xff;
    BOOST_CHECK(!VerifySeedManifestSignature(m, pub, "regtest"));
}

BOOST_AUTO_TEST_CASE(parse_rejects_malformed)
{
    SeedManifest m;
    std::string err;

    BOOST_CHECK(!ParseSeedManifestJson("not json", m, err));
    BOOST_CHECK(!ParseSeedManifestJson(R"({"epoch":"2026Q3"})", m, err));

    const std::string float_expiry = R"({
        "epoch": "2026Q3",
        "expiry": 1.5,
        "nodes": ["203.0.113.10:38732"],
        "signature": ")" + std::string(128, '0') + R"("})";
    BOOST_CHECK(!ParseSeedManifestJson(float_expiry, m, err));
    BOOST_CHECK(err.find("integer") != std::string::npos);

    const std::string zero_expiry = strprintf(R"({
        "epoch": "2026Q3",
        "expiry": 0,
        "nodes": ["203.0.113.10:38732"],
        "signature": "%s"
    })", std::string(128, '0'));
    BOOST_CHECK(!ParseSeedManifestJson(zero_expiry, m, err));
    BOOST_CHECK(err.find("positive") != std::string::npos);

    const std::string newline_epoch = strprintf(R"({
        "epoch": "2026Q3\nnext",
        "expiry": 1798761600,
        "nodes": ["203.0.113.10:38732"],
        "signature": "%s"
    })", std::string(128, '0'));
    BOOST_CHECK(!ParseSeedManifestJson(newline_epoch, m, err));

    const std::string bad_sig = R"({
        "epoch": "2026Q3",
        "expiry": 1798761600,
        "nodes": ["203.0.113.10:38732"],
        "signature": "00"
    })";
    BOOST_CHECK(!ParseSeedManifestJson(bad_sig, m, err));
    BOOST_CHECK(err.find("64 bytes") != std::string::npos);

    const std::string dup = strprintf(R"({
        "epoch": "2026Q3",
        "expiry": 1798761600,
        "nodes": ["203.0.113.10:38732", "203.0.113.10:38732"],
        "signature": "%s"
    })", std::string(128, '0'));
    BOOST_CHECK(!ParseSeedManifestJson(dup, m, err));
    BOOST_CHECK(err.find("duplicate") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_rejects_oversized_nodes)
{
    SeedManifest m;
    std::string err;
    std::string nodes = "[";
    for (size_t i = 0; i < MAX_SEED_MANIFEST_NODES + 1; ++i) {
        if (i) nodes += ",";
        nodes += strprintf("\"203.0.113.%u:38732\"", static_cast<unsigned>(i % 250) + 1);
    }
    nodes += "]";
    const std::string json = strprintf(R"({
        "epoch": "2026Q3",
        "expiry": 1798761600,
        "nodes": %s,
        "signature": "%s"
    })", nodes, std::string(128, '0'));
    BOOST_CHECK(!ParseSeedManifestJson(json, m, err));
    BOOST_CHECK(err.find("too many nodes") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(parse_and_epoch)
{
    SeedManifest m;
    std::string err;
    const std::string json_ok = strprintf(R"({
        "epoch": "2026Q3",
        "expiry": 1798761600,
        "nodes": ["203.0.113.10:38732"],
        "signature": "%s"
    })", std::string(128, '0'));
    BOOST_REQUIRE(ParseSeedManifestJson(json_ok, m, err));
    BOOST_CHECK_EQUAL(m.epoch, "2026Q3");
    BOOST_CHECK_EQUAL(m.expiry, 1798761600);
    BOOST_REQUIRE_EQUAL(m.nodes.size(), 1U);

    BOOST_CHECK(SeedEpochAccepted("2026Q3", {"2026Q3", "2026Q4"}));
    BOOST_CHECK(SeedEpochAccepted("2026Q4", {"2026Q3", "2026Q4"}));
    BOOST_CHECK(!SeedEpochAccepted("2025Q1", {"2026Q3", "2026Q4"}));

    const auto addrs = AddressesFromSeedManifest(m, 38732);
    BOOST_REQUIRE_EQUAL(addrs.size(), 1U);
    BOOST_CHECK_EQUAL(addrs[0].GetPort(), 38732);
}

BOOST_AUTO_TEST_SUITE_END()
