// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/convergenceres.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(convergence_fee_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pow_convergence_one_origo_matches_beta_weight_lock)
{
    const arith_uint256 w = Consensus::PowConvergenceAmount(COIN);
    // Consensus lock: W = floor(convergence_amount^(7/5)) via ConvergenceAmountBetaWeight.
    // For convergence_amount = 1 ORU (COIN base units), keep this deterministic reference.
    arith_uint256 expect;
    expect.SetHex("00000000000000000000000000000000000000000000000000000024e6b3174e");
    BOOST_CHECK_EQUAL(w.GetHex(), expect.GetHex());
}

BOOST_AUTO_TEST_CASE(convergence_draw_candidate_weight_scales_with_beta)
{
    const uint64_t w1 = Consensus::ConvergenceDrawEffectiveWeightU64(COIN);
    const uint64_t w10 = Consensus::ConvergenceDrawEffectiveWeightU64(10 * COIN);
    BOOST_CHECK(w10 > w1);
}

BOOST_AUTO_TEST_SUITE_END()
