// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <consensus/fingerprint.h>
#include <crypto/common.h>
#include <hash.h>
#include <span.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(consensus_fingerprint_tests)

namespace {

struct FingerprintVector {
    const char* name;
    const char* convergence_root;
    const char* winner_txid;
    const char* sigma;
    uint32_t height;
    uint32_t target_prev;
    const char* expected_fingerprint;
};

/** Hex fields are native serialization (digest order): first pair is byte 0. */
static uint256 DigestHexToUint256(const std::string& hex_str)
{
    const auto bytes = ParseHex(hex_str);
    BOOST_REQUIRE_EQUAL(bytes.size(), 32U);
    return uint256(Span<const unsigned char>(bytes.data(), bytes.size()));
}

/**
 * Published vectors from test_vectors/fingerprint_vectors.json.
 * Values must stay byte-identical to that file.
 */
static const FingerprintVector kVectors[] = {
    {
        "all_zero_height_100",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "0000000000000000000000000000000000000000000000000000000000000000",
        "0000000000000000000000000000000000000000000000000000000000000000",
        100,
        0x00000000,
        "a58b9a1f2d0f3994ef8e81b08258ae0ef130a4f234b1e331f4ee4efef2ad35d4",
    },
    {
        "distinct_fields",
        "0101010101010101010101010101010101010101010101010101010101010101",
        "0202020202020202020202020202020202020202020202020202020202020202",
        "0303030303030303030303030303030303030303030303030303030303030303",
        1,
        0x1d00ffff,
        "632cc6d0d5ab86a2fa74219cdf6f3f1cef91b71abc4aa34e52a496303e4adb8d",
    },
    {
        "sequential_bytes",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f",
        "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f",
        210000,
        0x1c0fffff,
        "062925341840c6feac9f86f9964f6d35fff81088c51b358c88e78cb374e01950",
    },
    {
        "max_uint32s",
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "5555555555555555555555555555555555555555555555555555555555555555",
        0xffffffff,
        0xffffffff,
        "50433a0d157a3e8d9184454b65ee15da1aa8cc0b311883c0884b0adddbb80001",
    },
    {
        "genesis_height_zero_target",
        "1111111111111111111111111111111111111111111111111111111111111111",
        "2222222222222222222222222222222222222222222222222222222222222222",
        "3333333333333333333333333333333333333333333333333333333333333333",
        0,
        0x00000000,
        "fff640bb8a6303085c03db6706a80867bf696fbe0667f137fd9ef6553247693c",
    },
};

static uint256 Hash256NativePreimage(
    const uint256& convergence_root,
    const uint256& winner_txid,
    const uint256& sigma,
    uint32_t height,
    uint32_t target_prev)
{
    std::array<unsigned char, CONSENSUS_FINGERPRINT_INPUT_SIZE> buf{};
    std::memcpy(buf.data() + 0, convergence_root.begin(), 32);
    std::memcpy(buf.data() + 32, winner_txid.begin(), 32);
    std::memcpy(buf.data() + 64, sigma.begin(), 32);
    WriteLE32(buf.data() + 96, height);
    WriteLE32(buf.data() + 100, target_prev);
    uint256 out;
    CHash256().Write(buf).Finalize(out);
    return out;
}

} // namespace

BOOST_AUTO_TEST_CASE(published_vectors_match)
{
    for (const auto& v : kVectors) {
        const uint256 got = ComputeConsensusFingerprint(
            DigestHexToUint256(v.convergence_root),
            DigestHexToUint256(v.winner_txid),
            DigestHexToUint256(v.sigma),
            v.height,
            v.target_prev);
        BOOST_CHECK_MESSAGE(
            HexStr(got) == v.expected_fingerprint,
            std::string(v.name) + " expected " + v.expected_fingerprint + " got " + HexStr(got));
    }
}

BOOST_AUTO_TEST_CASE(identical_input_identical_fingerprint)
{
    const FingerprintVector& v = kVectors[1];
    const uint256 a = ComputeConsensusFingerprint(
        DigestHexToUint256(v.convergence_root),
        DigestHexToUint256(v.winner_txid),
        DigestHexToUint256(v.sigma),
        v.height,
        v.target_prev);
    const uint256 b = ComputeConsensusFingerprint(
        DigestHexToUint256(v.convergence_root),
        DigestHexToUint256(v.winner_txid),
        DigestHexToUint256(v.sigma),
        v.height,
        v.target_prev);
    BOOST_CHECK_EQUAL(HexStr(a), HexStr(b));
    BOOST_CHECK_EQUAL(HexStr(a), v.expected_fingerprint);
}

BOOST_AUTO_TEST_CASE(matches_direct_hash256)
{
    const FingerprintVector& v = kVectors[2];
    const uint256 root = DigestHexToUint256(v.convergence_root);
    const uint256 txid = DigestHexToUint256(v.winner_txid);
    const uint256 sigma = DigestHexToUint256(v.sigma);
    const uint256 via_api = ComputeConsensusFingerprint(root, txid, sigma, v.height, v.target_prev);
    const uint256 via_hash = Hash256NativePreimage(root, txid, sigma, v.height, v.target_prev);
    BOOST_CHECK_EQUAL(HexStr(via_api), HexStr(via_hash));
    BOOST_CHECK_EQUAL(HexStr(via_api), v.expected_fingerprint);
}

BOOST_AUTO_TEST_CASE(field_change_changes_fingerprint)
{
    const uint256 root = DigestHexToUint256(kVectors[1].convergence_root);
    const uint256 txid = DigestHexToUint256(kVectors[1].winner_txid);
    const uint256 sigma = DigestHexToUint256(kVectors[1].sigma);
    const uint256 base = ComputeConsensusFingerprint(root, txid, sigma, 1, 0x1d00ffff);
    const uint256 height_moved = ComputeConsensusFingerprint(root, txid, sigma, 2, 0x1d00ffff);
    const uint256 target_moved = ComputeConsensusFingerprint(root, txid, sigma, 1, 0x1d00fffe);
    const uint256 swapped = ComputeConsensusFingerprint(txid, root, sigma, 1, 0x1d00ffff);
    BOOST_CHECK(base != height_moved);
    BOOST_CHECK(base != target_moved);
    BOOST_CHECK(base != swapped);
}

BOOST_AUTO_TEST_CASE(preimage_size_is_104)
{
    BOOST_CHECK_EQUAL(CONSENSUS_FINGERPRINT_INPUT_SIZE, 104U);
}

BOOST_AUTO_TEST_SUITE_END()
