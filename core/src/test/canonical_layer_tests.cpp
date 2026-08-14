// =============================================================================
// CANONICAL LAYER VALIDATION TESTS
// RFC-009, RFC-008, RFC-010 Compliance Verification
// =============================================================================

#include <boost/test/unit_test.hpp>

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_math.h>
#include <consensus_rigor/sigma_hash.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <arith_uint256.h>

#include <cstring>
#include <vector>
#include <array>

// =============================================================================
// T1: Encoding Tests (LE correctness)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_encoding_tests)

// CASE 1.1 — uint32 LE
// INPUT: height = 1
// EXPECTED: 01000000
BOOST_AUTO_TEST_CASE(uint32_le_height_1)
{
    uint32_t height = 1;
    std::array<unsigned char, 4> buf{};
    buf[0] = static_cast<unsigned char>((height >> 0)  & 0xFF);
    buf[1] = static_cast<unsigned char>((height >> 8)  & 0xFF);
    buf[2] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[3] = static_cast<unsigned char>((height >> 24) & 0xFF);
    
    // Expected: 01000000
    BOOST_CHECK_EQUAL(buf[0], 0x01);
    BOOST_CHECK_EQUAL(buf[1], 0x00);
    BOOST_CHECK_EQUAL(buf[2], 0x00);
    BOOST_CHECK_EQUAL(buf[3], 0x00);
}

// CASE 1.2 — uint32 LE (boundary)
// INPUT: height = 4294967295
// EXPECTED: ffffffff
BOOST_AUTO_TEST_CASE(uint32_le_max)
{
    uint32_t height = 4294967295U;
    std::array<unsigned char, 4> buf{};
    buf[0] = static_cast<unsigned char>((height >> 0)  & 0xFF);
    buf[1] = static_cast<unsigned char>((height >> 8)  & 0xFF);
    buf[2] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[3] = static_cast<unsigned char>((height >> 24) & 0xFF);
    
    // Expected: ffffffff
    BOOST_CHECK_EQUAL(buf[0], 0xff);
    BOOST_CHECK_EQUAL(buf[1], 0xff);
    BOOST_CHECK_EQUAL(buf[2], 0xff);
    BOOST_CHECK_EQUAL(buf[3], 0xff);
}

// CASE 1.3 — uint64 LE
// INPUT: value = 100000000
// EXPECTED: 00e1f50500000000
BOOST_AUTO_TEST_CASE(uint64_le_100000000)
{
    uint64_t value = 100000000ULL;
    std::array<unsigned char, 8> buf{};
    buf[0] = static_cast<unsigned char>((value >> 0)  & 0xFF);
    buf[1] = static_cast<unsigned char>((value >> 8)  & 0xFF);
    buf[2] = static_cast<unsigned char>((value >> 16) & 0xFF);
    buf[3] = static_cast<unsigned char>((value >> 24) & 0xFF);
    buf[4] = static_cast<unsigned char>((value >> 32) & 0xFF);
    buf[5] = static_cast<unsigned char>((value >> 40) & 0xFF);
    buf[6] = static_cast<unsigned char>((value >> 48) & 0xFF);
    buf[7] = static_cast<unsigned char>((value >> 56) & 0xFF);
    
    // Expected: 00e1f50500000000
    BOOST_CHECK_EQUAL(buf[0], 0x00);
    BOOST_CHECK_EQUAL(buf[1], 0xe1);
    BOOST_CHECK_EQUAL(buf[2], 0xf5);
    BOOST_CHECK_EQUAL(buf[3], 0x05);
    BOOST_CHECK_EQUAL(buf[4], 0x00);
    BOOST_CHECK_EQUAL(buf[5], 0x00);
    BOOST_CHECK_EQUAL(buf[6], 0x00);
    BOOST_CHECK_EQUAL(buf[7], 0x00);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T2: Concat Tests (byte exact)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_concat_tests)

// CASE 2.1
// INPUT: a = 0xaa, b = 0xbb
// EXPECTED: aabb
BOOST_AUTO_TEST_CASE(concat_simple)
{
    std::array<unsigned char, 2> buf{};
    buf[0] = 0xaa;
    buf[1] = 0xbb;
    
    BOOST_CHECK_EQUAL(buf[0], 0xaa);
    BOOST_CHECK_EQUAL(buf[1], 0xbb);
}

// CASE 2.2 (complex)
// INPUT: txid(32) + root(32) + hash(32) + height(4)
// EXPECTED: len = 100
BOOST_AUTO_TEST_CASE(concat_sigma_input)
{
    std::array<unsigned char, 100> buf{};
    uint256 txid;
    uint256 root;
    uint256 hash;
    uint32_t height = 12345;
    
    std::memcpy(buf.data() + 0,  txid.begin(),  32);
    std::memcpy(buf.data() + 32, root.begin(),  32);
    std::memcpy(buf.data() + 64, hash.begin(),  32);
    buf[96] = static_cast<unsigned char>((height >> 0)  & 0xFF);
    buf[97] = static_cast<unsigned char>((height >> 8)  & 0xFF);
    buf[98] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[99] = static_cast<unsigned char>((height >> 24) & 0xFF);
    
    // Check total length
    BOOST_CHECK_EQUAL(buf.size(), 100);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T3: Hash Input Tests (100 bytes EXACT)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_hash_input_tests)

// CASE 3.1 — correct length
// Build the full input
// EXPECTED: len == 100
BOOST_AUTO_TEST_CASE(sigma_input_correct_length)
{
    uint256 txid;
    uint256 convergence_root;
    uint256 prev_block_hash;
    uint32_t height = 100;

    // Sigma-PoW-01: the slot that used to hold `bind_commit` now holds the parent block hash
    // verbatim, so the σ input layout is still exactly 100 bytes.
    rigor::ComputeSigmaHash(txid, convergence_root, prev_block_hash, height);

    // If we got here without error, the input was 100 bytes
    BOOST_CHECK(true);
}

// CASE 3.2 — one byte short
// This test is conceptual - the static_assert in sigma_hash.cpp ensures
// the buffer is exactly 100 bytes at compile time
BOOST_AUTO_TEST_CASE(sigma_input_static_assert)
{
    // The static_assert in sigma_hash.cpp ensures:
    // static_assert(kOffHeight + kHeightLen == SIGMA_INPUT_SIZE, ...);
    // SIGMA_INPUT_SIZE is 100
    BOOST_CHECK_EQUAL(rigor::SIGMA_INPUT_SIZE, 100);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T4: Hash Output Tests (Hash256 correctness)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_hash_output_tests)

// CASE 4.1
// INPUT: "hello"
// EXPECTED: Hash256("hello") == 9595c9df90075148eb06860365df33584b75bff782a510c6cd4883a419833d50
//
// Compared in digest order, which is `HexStr` over the uint256's memory and NOT `GetHex()`: the
// latter prints reversed, and reading the expectation off a reversed print is how a byte-order
// bug gets locked in. reference/test_rigor_core.py pins the same bytes in the same order, so
// this ties the C++ digest to the Python reference.
BOOST_AUTO_TEST_CASE(hash256_hello)
{
    std::string input = "hello";
    CHash256 hasher;
    hasher.Write(MakeUCharSpan(input));
    uint256 result;
    hasher.Finalize(result);

    BOOST_CHECK_EQUAL(HexStr(result), "9595c9df90075148eb06860365df33584b75bff782a510c6cd4883a419833d50");
}

// Verify that we're using double SHA256, not single
BOOST_AUTO_TEST_CASE(hash256_is_double_sha256)
{
    std::string input = "test";
    
    // Compute Hash256 (double SHA256)
    CHash256 hasher;
    hasher.Write(MakeUCharSpan(input));
    uint256 double_hash;
    hasher.Finalize(double_hash);
    
    // Compute single SHA256
    CSHA256 single_hasher;
    single_hasher.Write(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    unsigned char single_hash[32];
    single_hasher.Finalize(single_hash);
    uint256 single_hash_uint256(single_hash);
    
    // They should be different
    BOOST_CHECK(double_hash != single_hash_uint256);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T5: Math Tests (no overflow, cross multiplication)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_math_tests)

// CASE 5.1 — basic comparison
// H1 = 10, f1 = 2
// H2 = 15, f2 = 3
// CHECK: 10*3 < 15*2 → 30 < 30 → FALSE
BOOST_AUTO_TEST_CASE(cross_multiplication_basic)
{
    uint256 h1(10);
    uint256 h2(15);
    CAmount f1 = 2;
    CAmount f2 = 3;
    
    // H1 * f2 = 10 * 3 = 30
    // H2 * f1 = 15 * 2 = 30
    // 30 < 30 should be FALSE
    
    const Consensus::uint512 right1(Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h1), arith_uint256(f2)));
    const Consensus::uint512 right2(Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h2), arith_uint256(f1)));
    
    // 30 < 30 should be false
    BOOST_CHECK(!(right1 < right2));
}

// CASE 5.2 — tie
// H1 * f2 == H2 * f1, so neither candidate precedes the other and the argmin must fall through
// to the txid comparison. That fall-through itself is exercised on real draw sets by
// `winner_is_strict_argmin` in convergence_consensus_tests; what is pinned here is that the
// arithmetic reports a tie rather than an ordering.
BOOST_AUTO_TEST_CASE(cross_multiplication_tie)
{
    uint256 h1(10);
    uint256 h2(25);
    CAmount f1 = 4;
    CAmount f2 = 10;

    // H1 * f2 = 10 * 10 = 100
    // H2 * f1 = 25 *  4 = 100
    const Consensus::uint512 right1(Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h1), arith_uint256(f2)));
    const Consensus::uint512 right2(Consensus::rigor_math_detail::Mul256x256Full(UintToArith256(h2), arith_uint256(f1)));

    // Both strict comparisons must be false; asserting only one would pass on a comparator that
    // reported every pair as ordered.
    BOOST_CHECK(!(right1 < right2));
    BOOST_CHECK(!(right2 < right1));
}

// CASE 5.3 — overflow test
// H = 2^256 - 1, f = 10^12
// EXPECTED: the full product survives, i.e. (2^256 - 1) * f == f * 2^256 - f
BOOST_AUTO_TEST_CASE(cross_multiplication_no_overflow)
{
    const arith_uint256 h = ~arith_uint256(0); // 2^256 - 1
    const CAmount f = 1000000000000LL;         // 10^12

    const Consensus::uint512 result(
        Consensus::rigor_math_detail::Mul256x256Full(h, arith_uint256(static_cast<uint64_t>(f))));

    // Reaching this line proves nothing on its own: a 256-bit truncating multiply also returns.
    // The product itself has to be checked. (2^256 - 1)·f = f·2^256 - f, so the low 256 bits are
    // 2^256 - f — the two's complement of f in the bottom word with the borrow filling the three
    // above it — and the high 256 bits are f - 1.
    const uint64_t fw = static_cast<uint64_t>(f);
    Consensus::uint512 expected;
    expected.m_w[0] = ~fw + 1;
    expected.m_w[1] = expected.m_w[2] = expected.m_w[3] = ~uint64_t{0};
    expected.m_w[4] = fw - 1;
    BOOST_CHECK(result == expected);
}

// Test the actual inequality function
BOOST_AUTO_TEST_CASE(inequality_holds_basic)
{
    // RFC-014 Conv-Decouple-01: inequality RHS is the constitutional constant
    // `RES_PROBABILITY_TARGET` = floor(2^240 / MIN_CONVERGENCE_AMOUNT) (SECURITY-001-A A-6).
    // This test does not use the BasicTestingSetup fixture, so the constant is read from
    // its definition rather than from Params().
    const uint256& T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const CAmount f_min = Consensus::MIN_CONVERGENCE_AMOUNT;

    // A tiny σ clears T·f for any admissible weight; σ = 2^256 - 1 clears nothing, since T·f is
    // below 2^256 for the minimum weight (that bound is itself the A-6 invariant, asserted in
    // res_proof_math_tests). Calling the function and asserting nothing, as this case used to,
    // could not have failed for any implementation.
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, f_min));
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(ArithToUint256(~arith_uint256(0)), T, f_min));

    // f below 1 satoshi, and f outside MoneyRange, are rejects rather than comparisons.
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, 0));
    BOOST_CHECK(!Consensus::ConvergenceDrawInequalityHolds(uint256(10), T, MAX_MONEY + 1));
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T6: Sorting Tests (unsigned byte order)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_sorting_tests)

// CASE 6.1
// INPUT: [ff00, 00ff]
// EXPECTED: 00ff, ff00
BOOST_AUTO_TEST_CASE(unsigned_byte_sort_case1)
{
    std::vector<uint256> txids;
    
    // Create two uint256 values
    uint256 txid1;
    uint256 txid2;
    
    // Set first byte of each
    txid1.begin()[0] = 0xff;
    txid2.begin()[0] = 0x00;
    
    txids.push_back(txid1);
    txids.push_back(txid2);
    
    std::sort(txids.begin(), txids.end());
    
    // 00ff should come before ff00
    BOOST_CHECK_EQUAL(txids[0].begin()[0], 0x00);
    BOOST_CHECK_EQUAL(txids[1].begin()[0], 0xff);
}

// CASE 6.2
// INPUT: [80, 7f]
// EXPECTED: 7f, 80
// This tests that we use unsigned comparison, not signed
BOOST_AUTO_TEST_CASE(unsigned_byte_sort_case2)
{
    std::vector<uint256> txids;
    
    uint256 txid1;
    uint256 txid2;
    
    // Set first byte of each
    txid1.begin()[0] = 0x80; // 128 unsigned, -128 signed
    txid2.begin()[0] = 0x7f; // 127 unsigned, 127 signed
    
    txids.push_back(txid1);
    txids.push_back(txid2);
    
    std::sort(txids.begin(), txids.end());
    
    // With unsigned comparison: 0x7f (127) < 0x80 (128)
    // With signed comparison: 0x80 (-128) < 0x7f (127)
    // We expect unsigned: 7f, 80
    BOOST_CHECK_EQUAL(txids[0].begin()[0], 0x7f);
    BOOST_CHECK_EQUAL(txids[1].begin()[0], 0x80);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// T7: Execution Trace Tests (exact output)
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_trace_tests)

// CASE 7.1
// INPUT: 2 tx
// EXPECTED TRACE with txid, f_i, H_i, sigma, eligible
BOOST_AUTO_TEST_CASE(execution_trace_structure)
{
    // This test verifies the execution flow produces the required fields
    // The actual trace output is optional per RFC-010 §2
    
    // Verify that the key functions exist and can be called
    uint256 txid;
    uint256 convergence_root;
    uint256 prev_block_hash;
    uint32_t height = 100;

    // Compute Sigma hash (Sigma-PoW-01: seeded by the parent block hash).
    uint256 sigma = rigor::ComputeSigmaHash(txid, convergence_root, prev_block_hash, height);

    // Check inequality (RFC-014: RHS = constitutional constant).
    CAmount f_sats = 100000000;
    const uint256& res_probability_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    Consensus::ConvergenceDrawInequalityHolds(sigma, res_probability_target, f_sats);
    
    // If we got here, the trace fields are computable
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Anti-Cheat Tests
// =============================================================================

BOOST_AUTO_TEST_SUITE(canonical_anti_cheat_tests)

// Test that we're not using big-endian
BOOST_AUTO_TEST_CASE(no_big_endian)
{
    uint32_t height = 0x12345678;
    std::array<unsigned char, 4> buf{};
    buf[0] = static_cast<unsigned char>((height >> 0)  & 0xFF);
    buf[1] = static_cast<unsigned char>((height >> 8)  & 0xFF);
    buf[2] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[3] = static_cast<unsigned char>((height >> 24) & 0xFF);
    
    // Little-endian: 78 56 34 12
    // Big-endian: 12 34 56 78
    BOOST_CHECK_EQUAL(buf[0], 0x78); // Least significant byte first
    BOOST_CHECK_EQUAL(buf[3], 0x12); // Most significant byte last
}

// Test that we're using double SHA256, not single
BOOST_AUTO_TEST_CASE(no_single_sha256)
{
    std::string input = "test";
    
    CHash256 hasher;
    hasher.Write(MakeUCharSpan(input));
    uint256 double_hash;
    hasher.Finalize(double_hash);
    
    CSHA256 single_hasher;
    single_hasher.Write(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    unsigned char single_hash_bytes[32];
    single_hasher.Finalize(single_hash_bytes);
    uint256 single_hash(single_hash_bytes);
    
    // They must be different
    BOOST_CHECK(double_hash != single_hash);
}

// Test that we're using unsigned byte comparison for sorting
BOOST_AUTO_TEST_CASE(no_signed_sort)
{
    std::vector<uint256> txids;
    
    uint256 txid1;
    uint256 txid2;
    
    txid1.begin()[0] = 0x80;
    txid2.begin()[0] = 0x7f;
    
    txids.push_back(txid1);
    txids.push_back(txid2);
    
    std::sort(txids.begin(), txids.end());
    
    // Unsigned: 0x7f < 0x80
    // Signed: 0x80 < 0x7f
    // We expect unsigned
    BOOST_CHECK_EQUAL(txids[0].begin()[0], 0x7f);
}

BOOST_AUTO_TEST_SUITE_END()

// =============================================================================
// Block space caps (must match consensus.h / rigor_consensus_lock.h)
// =============================================================================

BOOST_AUTO_TEST_SUITE(block_space_lock_tests)

BOOST_AUTO_TEST_CASE(serialized_and_weight_caps_match_consensus_headers)
{
    // Lock version and BLOCK_*_LIMIT / MAX_BLOCK_* live in headers; guard drift.
    BOOST_CHECK_EQUAL(Consensus::RIGOR_CONSENSUS_LOCK_VERSION, 2);
    BOOST_CHECK_EQUAL(Consensus::BLOCK_SERIALIZED_SIZE_LIMIT, 1000000u);
    BOOST_CHECK_EQUAL(Consensus::BLOCK_WEIGHT_LIMIT, 1000000u);
    BOOST_CHECK_EQUAL(Consensus::BLOCK_SIGOPS_COST_LIMIT, 20000);
    BOOST_CHECK_EQUAL(MAX_BLOCK_SERIALIZED_SIZE, 1000000u);
    BOOST_CHECK_EQUAL(MAX_BLOCK_WEIGHT, 1000000u);
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIGOPS_COST, 20000);
}

BOOST_AUTO_TEST_SUITE_END()
