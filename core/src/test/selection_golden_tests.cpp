// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <consensus/selection.h>
#include <consensus_rigor/sigma_hash.h>
#include <hash.h>
#include <span.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(selection_golden_tests)

/**
 * Golden Test Vector Suite
 * RFC-006 SUBSET
 *
 * THIS FILE DEFINES CANONICAL CONSENSUS OUTPUTS.
 * ANY IMPLEMENTATION MUST MATCH ALL OUTPUTS EXACTLY.
 * BYTE-FOR-BYTE MATCH IS REQUIRED.
 * NO TOLERANCE.
 * NO ROUNDING.
 * NO INTERPRETATION.
 * IF ANY TEST FAILS → IMPLEMENTATION IS INVALID.
 */

// Helper: parse a hex string as a uint256 in *Bitcoin display order*
// (i.e. SetHex semantics: hex high-order character on the left maps to
// m_data[31]). Used for opaque input txids / prev hashes where the historical
// hex orientation must be preserved.
static uint256 ParseHex256(const std::string& hex_str)
{
    uint256 result;
    result.SetHex(hex_str);
    return result;
}

// Helper: parse a hex string as the *raw byte sequence* of a Hash256 output
// (no endian reversal). CHash256::Finalize writes hash bytes directly into
// uint256::m_data, so when comparing against a Python-computed
// `hashlib.sha256(sha256(x)).hexdigest()`, the resulting uint256 MUST be
// constructed from the hex bytes in forward order (m_data[0] = first hash
// byte). This is the opposite convention from SetHex (which reverses).
static uint256 HashHexToUint256(const std::string& hex_str)
{
    auto bytes = ParseHex<unsigned char>(hex_str);
    assert(bytes.size() == 32);
    return uint256(Span<const unsigned char>(bytes.data(), bytes.size()));
}

/**
 * GOLDEN VECTOR PROVENANCE
 * ------------------------
 * The σ values below were regenerated for SECURITY-001-A8 (Sigma-PoW-01):
 *
 *     σ = Hash256(txid || C || prev_block_hash || LE32(height))
 *
 * `target_prev` is gone from the preimage — the parent header commits its own
 * nBits, so folding it in separately added nothing. Every ConvergenceRoot below
 * is unchanged from the previous revision, since A-8 did not touch how C is
 * built; only the σ column moved.
 *
 * The inequality RHS remains the constitutional constant
 * `Consensus::Params::RES_PROBABILITY_TARGET` (RFC-014 Conv-Decouple-01); the
 * cases pass explicit targets to exercise its boundaries.
 *
 * `compute_rfc001_vectors.cpp` regenerates these.
 *
 * CASE 001 — empty set
 */

BOOST_AUTO_TEST_CASE(empty_set)
{
    std::vector<uint256> txids;
    std::vector<uint64_t> fees;
    uint32_t height = 1;
    uint256 prev_hash = uint256::ZERO;
    uint256 target = uint256::ONE; // 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff

    // Run selection
    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Expected outputs
    uint256 expected_C = uint256::ZERO;
    std::vector<uint256> expected_sigma;
    std::vector<bool> expected_eligible;
    int expected_winner_index = -1; // null

    // Verify exact byte-for-byte match
    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), expected_sigma.size());
    BOOST_CHECK_EQUAL(result.eligible.size(), expected_eligible.size());
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 003 — Sorting Order
 * Computed golden vector
 */
BOOST_AUTO_TEST_CASE(sorting_order)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
    txids.push_back(ParseHex256("0000000000000000000000000000000000000000000000000000000000000000"));
    
    std::vector<uint64_t> fees = {1, 1};
    uint32_t height = 10;
    uint256 prev_hash = ParseHex256("1111111111111111111111111111111111111111111111111111111111111111");
    uint256 target = ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Sigma-PoW-01 golden vector.
    // With equal fees=1, smaller σ wins, so winner is the txid whose σ is
    // smallest — here index 0 (σ_0 < σ_1).
    // Hex strings below are *forward-order* (Python `hash.hex()` orientation):
    // m_data[0] = first hash byte. Use HashHexToUint256 (not ParseHex256).
    uint256 expected_C = HashHexToUint256("4fa4a869878bd4837e6192377293a0cc14f192b88711635ca3df0447dc1bea76");
    uint256 expected_sigma_0 = HashHexToUint256("9b8fec9a657ea2e6d27f3eead98c02cca2f8c941970ed60b3238d08f4dbd75b2");
    uint256 expected_sigma_1 = HashHexToUint256("6e0f00f65387c23518af80e3b215f895b552d33f3ed90c653a083307511e0cc1");
    std::vector<bool> expected_eligible = {true, true};
    int expected_winner_index = 0;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 2);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma_0);
    BOOST_CHECK(result.sigma_hashes[1] == expected_sigma_1);
    BOOST_CHECK_EQUAL(result.eligible.size(), 2);
    BOOST_CHECK(result.eligible[0] == expected_eligible[0]);
    BOOST_CHECK(result.eligible[1] == expected_eligible[1]);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 002 — single_tx_winner
 * RFC-015: IF N >= 1 and inequality holds, singleton candidate may win.
 * Sigma-PoW-01 golden vector
 */
BOOST_AUTO_TEST_CASE(single_tx_winner)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    std::vector<uint64_t> fees = {1};
    uint32_t height = 1;
    uint256 prev_hash = ParseHex256("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    uint256 target = ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Singleton F hashes: C = Hash256(txid). Empty F remains the zero root.
    uint256 expected_C = HashHexToUint256("6dc8daeb8077a87be2712c5c8cceeed7a2d2e20c8b2e33adfefabccb32877a4d");
    uint256 expected_sigma = HashHexToUint256("2d28080da537ba65fee631d7b68207599d3c08f2b7cfed007dde7495bc930b09");
    bool expected_eligible = true;
    int expected_winner_index = 0;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 1);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma);
    BOOST_CHECK_EQUAL(result.eligible.size(), 1);
    BOOST_CHECK(result.eligible[0] == expected_eligible);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 004 — no_winner
 * Computed golden vector
 */
BOOST_AUTO_TEST_CASE(no_winner)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    txids.push_back(ParseHex256("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    std::vector<uint64_t> fees = {1, 1};
    uint32_t height = 5;
    uint256 prev_hash = ParseHex256("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
    uint256 target = ParseHex256("0000000000000000000000000000000000000000000000000000000000000001");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Sigma-PoW-01 golden vector.
    uint256 expected_C = HashHexToUint256("499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb");
    uint256 expected_sigma_0 = HashHexToUint256("c3f728239a89f1876eef97bb9e9238516e8e34f34c8b6408edc63b2bc20aaf88");
    uint256 expected_sigma_1 = HashHexToUint256("3957870e0c706c61cc7ca73350828ae3d56f3f082cdcf10eb6f8fcf7ae45332b");
    std::vector<bool> expected_eligible = {false, false};
    int expected_winner_index = -1;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 2);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma_0);
    BOOST_CHECK(result.sigma_hashes[1] == expected_sigma_1);
    BOOST_CHECK_EQUAL(result.eligible.size(), 2);
    BOOST_CHECK(result.eligible[0] == expected_eligible[0]);
    BOOST_CHECK(result.eligible[1] == expected_eligible[1]);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 005 — weight_priority
 * Sigma-PoW-01 golden vector
 */
BOOST_AUTO_TEST_CASE(weight_priority)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    txids.push_back(ParseHex256("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    std::vector<uint64_t> fees = {1, 1000};
    uint32_t height = 7;
    uint256 prev_hash = ParseHex256("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    uint256 target = ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Sigma-PoW-01 golden vector.
    uint256 expected_C = HashHexToUint256("499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb");
    uint256 expected_sigma_0 = HashHexToUint256("4f19b2a6c8b63b1170d8ab52268deda9e5cd171e85549460beabf164b29192c7");
    uint256 expected_sigma_1 = HashHexToUint256("c8d3d18fc2f5f86630d734ef796bb4d048b846d2ddaa1b37284136ac54919061");
    std::vector<bool> expected_eligible = {true, true};
    int expected_winner_index = 1;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 2);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma_0);
    BOOST_CHECK(result.sigma_hashes[1] == expected_sigma_1);
    BOOST_CHECK_EQUAL(result.eligible.size(), 2);
    BOOST_CHECK(result.eligible[0] == expected_eligible[0]);
    BOOST_CHECK(result.eligible[1] == expected_eligible[1]);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 006 — tie_break
 * Sigma-PoW-01 golden vector
 */
BOOST_AUTO_TEST_CASE(tie_break)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("1111111111111111111111111111111111111111111111111111111111111111"));
    txids.push_back(ParseHex256("2222222222222222222222222222222222222222222222222222222222222222"));
    std::vector<uint64_t> fees = {10, 10};
    uint32_t height = 9;
    uint256 prev_hash = ParseHex256("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
    uint256 target = ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Sigma-PoW-01 golden vector.
    uint256 expected_C = HashHexToUint256("1140b574afee3cb89a4db3dc8037acfa856f5112e68a954e3ca0a908082c98ba");
    uint256 expected_sigma_0 = HashHexToUint256("fb47e7af1a7f9ed73f5e4a5aed5344e9f1bf7fabbef31d2839e2766e8322dbee");
    uint256 expected_sigma_1 = HashHexToUint256("717ef2a6325670be2f6714de2687d9b0399c9062c1b683ea1f0150b99859a852");
    std::vector<bool> expected_eligible = {true, true};
    int expected_winner_index = 1;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 2);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma_0);
    BOOST_CHECK(result.sigma_hashes[1] == expected_sigma_1);
    BOOST_CHECK_EQUAL(result.eligible.size(), 2);
    BOOST_CHECK(result.eligible[0] == expected_eligible[0]);
    BOOST_CHECK(result.eligible[1] == expected_eligible[1]);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}

/**
 * CASE 007 — big_int
 * Sigma-PoW-01 golden vector
 */
BOOST_AUTO_TEST_CASE(big_int)
{
    std::vector<uint256> txids;
    txids.push_back(ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
    txids.push_back(ParseHex256("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"));
    std::vector<uint64_t> fees = {18446744073709551615ULL, 1};  // UINT64_MAX, 1
    uint32_t height = 100;
    uint256 prev_hash = ParseHex256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    uint256 target = ParseHex256("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    Consensus::SelectionResult result = Consensus::RunSelection(
        txids, fees, height, prev_hash, target
    );

    // Sigma-PoW-01 golden vector.
    uint256 expected_C = HashHexToUint256("490de05febe1b32d530db9f4587a5eae06cbbe2dd0aa8582c0ab7755ebcb9d05");
    uint256 expected_sigma_0 = HashHexToUint256("1d3365ed0ea0262ad8d9ab85682124ee6e7c514723de97a00222e9942ac115b2");
    uint256 expected_sigma_1 = HashHexToUint256("a7b17ad1704130182b2c71db746dc769a338c6e508248ffa0dc84719a0266193");
    std::vector<bool> expected_eligible = {true, true};
    int expected_winner_index = 0;

    BOOST_CHECK(result.convergence_root == expected_C);
    BOOST_CHECK_EQUAL(result.sigma_hashes.size(), 2);
    BOOST_CHECK(result.sigma_hashes[0] == expected_sigma_0);
    BOOST_CHECK(result.sigma_hashes[1] == expected_sigma_1);
    BOOST_CHECK_EQUAL(result.eligible.size(), 2);
    BOOST_CHECK(result.eligible[0] == expected_eligible[0]);
    BOOST_CHECK(result.eligible[1] == expected_eligible[1]);
    BOOST_CHECK_EQUAL(result.winner_index, expected_winner_index);
}


#include <test/selection_golden_bridge.inc>

static uint256 Uint256FromRawHex(const std::string& hex_str)
{
    return HashHexToUint256(hex_str);
}

BOOST_AUTO_TEST_CASE(canonical_draw_encoder_layout)
{
    for (size_t i = 0; i < rigor_golden::kEncN; ++i) {
        const auto& v = rigor_golden::kEnc[i];
        const uint256 txid = Uint256FromRawHex(v.txid_hex);
        const uint256 croot = Uint256FromRawHex(v.croot_hex);
        const uint256 prev = Uint256FromRawHex(v.prev_hex);

        std::array<unsigned char, rigor::SIGMA_INPUT_SIZE> buf{};
        std::memcpy(buf.data() + 0, txid.begin(), 32);
        std::memcpy(buf.data() + 32, croot.begin(), 32);
        std::memcpy(buf.data() + 64, prev.begin(), 32);
        buf[96] = static_cast<unsigned char>((v.height >> 0) & 0xFF);
        buf[97] = static_cast<unsigned char>((v.height >> 8) & 0xFF);
        buf[98] = static_cast<unsigned char>((v.height >> 16) & 0xFF);
        buf[99] = static_cast<unsigned char>((v.height >> 24) & 0xFF);

        auto pre = ParseHex<unsigned char>(v.preimage_hex);
        BOOST_CHECK_MESSAGE(pre.size() == 100, v.id);
        BOOST_CHECK_MESSAGE(std::equal(pre.begin(), pre.end(), buf.begin()), v.id);

        uint256 hashed;
        CHash256().Write(buf).Finalize(hashed);
        BOOST_CHECK_MESSAGE(hashed == rigor::ComputeSigmaHash(txid, croot, prev, v.height), v.id);
        BOOST_CHECK_MESSAGE(hashed == Uint256FromRawHex(v.sigma_hex), v.id);
    }
}

BOOST_AUTO_TEST_CASE(a9c_already_paid_exclusion_golden)
{
    for (size_t i = 0; i < rigor_golden::kA9CN; ++i) {
        const auto& v = rigor_golden::kA9C[i];
        std::vector<std::string> got;
        for (const auto& cand : v.candidates) {
            if (std::find(v.paid.begin(), v.paid.end(), cand) == v.paid.end()) {
                got.push_back(cand);
            }
        }
        BOOST_CHECK_MESSAGE(got == v.surviving, v.id);
    }
}

BOOST_AUTO_TEST_CASE(sigma_hash_kernel_golden)
{
    for (size_t i = 0; i < rigor_golden::kSigN; ++i) {
        const auto& v = rigor_golden::kSig[i];
        const uint256 got = rigor::ComputeSigmaHash(
            Uint256FromRawHex(v.txid_hex),
            Uint256FromRawHex(v.croot_hex),
            Uint256FromRawHex(v.prev_hex),
            v.height);
        BOOST_CHECK_MESSAGE(got == Uint256FromRawHex(v.sigma_hex), v.id);
    }
}

BOOST_AUTO_TEST_CASE(sha256d_hash_kernel_golden)
{
    for (size_t i = 0; i < rigor_golden::kHashN; ++i) {
        const auto& v = rigor_golden::kHash[i];
        auto payload = ParseHex<unsigned char>(v.payload_hex);
        uint256 got;
        CHash256().Write(Span<const unsigned char>{payload.data(), payload.size()}).Finalize(got);
        BOOST_CHECK_MESSAGE(got == Uint256FromRawHex(v.sha256d_hex), v.id);
    }
}

BOOST_AUTO_TEST_CASE(sha256_fips_golden)
{
    for (size_t i = 0; i < rigor_golden::kSha256N; ++i) {
        const auto& v = rigor_golden::kSha256[i];
        auto payload = ParseHex<unsigned char>(v.payload_hex);
        unsigned char out[CSHA256::OUTPUT_SIZE];
        CSHA256().Write(payload.data(), payload.size()).Finalize(out);
        uint256 got(Span<const unsigned char>(out, CSHA256::OUTPUT_SIZE));
        BOOST_CHECK_MESSAGE(got == Uint256FromRawHex(v.sha256_hex), v.id);
    }
}

BOOST_AUTO_TEST_SUITE_END()
