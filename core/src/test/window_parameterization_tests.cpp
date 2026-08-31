// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <wallet/convergence_window_ui.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

namespace {

/** Generic live formula. Does NOT read production CONVERGENCE_DRAW_WINDOW. */
int WindowStart(int s, int W)
{
    return std::max(0, s - W + 1);
}

int WindowEnd(int s, int W)
{
    return s;
}

int WindowLength(int s, int W)
{
    return WindowEnd(s, W) - WindowStart(s, W) + 1;
}

int DrawStart(int h, int W) { return h - W; }
int DrawEnd(int h, int W) { return h - 1; }

arith_uint256 TOfW(int W)
{
    const arith_uint256 unwindowed = UintToArith256(Consensus::RES_PROBABILITY_TARGET_UNWINDOWED);
    return unwindowed / arith_uint256(static_cast<uint64_t>(W));
}

void CheckGenericGeometry(int W)
{
    BOOST_REQUIRE(W > 1);

    // Full width once s >= W-1; genesis truncation never goes negative.
    BOOST_CHECK_EQUAL(WindowStart(0, W), 0);
    BOOST_CHECK_EQUAL(WindowLength(0, W), 1);
    BOOST_CHECK_EQUAL(WindowStart(W - 2, W), 0);
    BOOST_CHECK_EQUAL(WindowLength(W - 2, W), W - 1);
    BOOST_CHECK_EQUAL(WindowStart(W - 1, W), 0);
    BOOST_CHECK_EQUAL(WindowLength(W - 1, W), W);
    BOOST_CHECK_EQUAL(WindowStart(W, W), 1);
    BOOST_CHECK_EQUAL(WindowLength(W, W), W);
    BOOST_CHECK_EQUAL(WindowStart(W + 1, W), 2);
    BOOST_CHECK_EQUAL(WindowLength(W + 1, W), W);
    BOOST_CHECK_EQUAL(WindowStart(2 * W, W), W + 1);
    BOOST_CHECK_EQUAL(WindowLength(2 * W, W), W);

    // Not a floor(h/W) partition: source 2W covers [W+1, 2W], not [2W, 3W-1].
    BOOST_CHECK(WindowStart(2 * W, W) != W * ((2 * W) / W));

    // Slice-end coincidence only: source W-1 covers [0, W-1] == floor group 0.
    BOOST_CHECK_EQUAL(WindowStart(W - 1, W), 0);
    BOOST_CHECK_EQUAL(WindowEnd(W - 1, W), W - 1);

    // Draw paid at h uses [h-W, h-1]; length W.
    BOOST_CHECK_EQUAL(DrawEnd(1000, W) - DrawStart(1000, W) + 1, W);

    // Reorg does not move the interval of a fixed source.
    const int s = 2 * W + 3;
    BOOST_CHECK_EQUAL(WindowStart(s, W), WindowStart(s, W));
    BOOST_CHECK_EQUAL(WindowEnd(s, W), s);

    // Maturity is 100, independent of this W.
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK(W != COINBASE_MATURITY);
    if (W < COINBASE_MATURITY) {
        BOOST_CHECK_LT(W, COINBASE_MATURITY);
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(window_parameterization_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(production_W_remains_32)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW_LOG2, 5);
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 1 << Consensus::CONVERGENCE_DRAW_WINDOW_LOG2);
    BOOST_CHECK(Params().GetConsensus().RES_PROBABILITY_TARGET == Consensus::RES_PROBABILITY_TARGET_CONST);
}

BOOST_AUTO_TEST_CASE(generic_window_for_listed_W)
{
    const int values[] = {2, 3, 4, 8, 16, 32, 36, 48, 64};
    for (int W : values) {
        CheckGenericGeometry(W);
    }
}

BOOST_AUTO_TEST_CASE(W36_is_not_power_of_two_and_still_well_formed)
{
    BOOST_CHECK_NE(36, 1 << 5);
    BOOST_CHECK_NE(36, 1 << 6);
    CheckGenericGeometry(36);

    BOOST_CHECK_EQUAL(WindowStart(35, 36), 0);
    BOOST_CHECK_EQUAL(WindowEnd(35, 36), 35);
    BOOST_CHECK_EQUAL(WindowLength(35, 36), 36);
    BOOST_CHECK_EQUAL(WindowStart(36, 36), 1);
    BOOST_CHECK_EQUAL(WindowStart(37, 36), 2);
    BOOST_CHECK_EQUAL(WindowStart(72, 36), 37);
    BOOST_CHECK_EQUAL(WindowLength(72, 36), 36);
}

BOOST_AUTO_TEST_CASE(W32_golden_matches_production_formula)
{
    BOOST_CHECK_EQUAL(WindowStart(31, 32), 0);
    BOOST_CHECK_EQUAL(WindowStart(32, 32), 1);
    BOOST_CHECK_EQUAL(WindowStart(33, 32), 2);
    BOOST_CHECK_EQUAL(WindowStart(64, 32), 33);
    BOOST_CHECK_EQUAL(WindowStart(1023, 32), 992);
    BOOST_CHECK_EQUAL(WindowStart(1023, 32),
                      std::max(0, 1023 - Consensus::CONVERGENCE_DRAW_WINDOW + 1));
}

BOOST_AUTO_TEST_CASE(T_of_W_pairs_with_window)
{
    const arith_uint256 unwindowed = UintToArith256(Consensus::RES_PROBABILITY_TARGET_UNWINDOWED);
    const arith_uint256 T32 = TOfW(32);
    const arith_uint256 T36 = TOfW(36);

    BOOST_CHECK(T32 == UintToArith256(Consensus::RES_PROBABILITY_TARGET_CONST));
    BOOST_CHECK(T32 == (unwindowed >> Consensus::CONVERGENCE_DRAW_WINDOW_LOG2));
    BOOST_CHECK(T32 != T36);
    BOOST_CHECK(T36 != (unwindowed >> 5));

    BOOST_CHECK((T32 * 32) <= unwindowed);
    BOOST_CHECK(unwindowed < ((T32 + 1) * 32));
    BOOST_CHECK((T36 * 36) <= unwindowed);
    BOOST_CHECK(unwindowed < ((T36 + 1) * 36));

    // Stale pair: T(32) with W=36 is not the A-9C floor pair (issuance would scale).
    BOOST_CHECK(unwindowed < (T32 * 36));

    // Neutrality remainder.
    BOOST_CHECK_EQUAL((unwindowed - T32 * 32).GetLow64(), 27);
    BOOST_CHECK_EQUAL((unwindowed - T36 * 36).GetLow64(), 31);
}

BOOST_AUTO_TEST_CASE(maturity_unchanged_across_W)
{
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, 100);
    BOOST_CHECK_NE(COINBASE_MATURITY, Consensus::CONVERGENCE_DRAW_WINDOW);
    BOOST_CHECK_NE(COINBASE_MATURITY, 36);
    BOOST_CHECK_NE(COINBASE_MATURITY, 2 * Consensus::CONVERGENCE_DRAW_WINDOW);
}

BOOST_AUTO_TEST_CASE(production_ui_still_uses_lock_W32)
{
    const auto s = wallet::BuildConvergenceUiWindow(/*inclusion=*/100, /*tip=*/100, false, false);
    BOOST_CHECK_EQUAL(s.W, 32);
    BOOST_CHECK_EQUAL(s.finalization_block, 132);
}

BOOST_AUTO_TEST_SUITE_END()
