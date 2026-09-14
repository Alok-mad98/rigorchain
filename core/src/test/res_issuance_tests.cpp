// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_issuance.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <limits>

/**
 * RFC-020 continuous destroy cost, closed-form cap and mint predicate.
 *
 * res_issuance.h carries most of its own contract as static_asserts. Runtime
 * covers ranges a static_assert cannot loop over, and the traps that only
 * show up when the whole domain is swept.
 *
 * Cross-implementation agreement is checked by
 * tools/res_continuous_price_schedule.py (numeric SSOT).
 */
BOOST_AUTO_TEST_SUITE(res_issuance_tests)

using namespace Consensus;

BOOST_AUTO_TEST_CASE(display_price_is_monotone)
{
    const int probes[] = {0, 1, 2, 1000, 52594, static_cast<int>(RES_DESTROY_POLE) - 1,
                          static_cast<int>(RES_DESTROY_POLE), static_cast<int>(RES_DESTROY_POLE) + 1,
                          1 << 20};
    for (int h : probes) {
        BOOST_CHECK_LE(ResDestroyCostAtHeight(h), ResDestroyCostAtHeight(h + 1));
        BOOST_CHECK_GT(ResDestroyCostAtHeight(h), 0);
        BOOST_CHECK_LE(ResMintAtomsFromBurn(ORU_FLAT_BLOCK_SUBSIDY, h + 1),
                       ResMintAtomsFromBurn(ORU_FLAT_BLOCK_SUBSIDY, h));
    }
    BOOST_CHECK_GT(ResDestroyCostAtHeight(1 << 20), ResDestroyCostAtHeight(0));
}

BOOST_AUTO_TEST_CASE(predicate_matches_exact_ceil_not_display_price)
{
    // Display destroy cost is floor-divided. The exact burn that buys one whole RES
    // can sit one atom above it. Consensus uses the uncleared inequality.
    const int heights[] = {0, 1, 1000, 52594, static_cast<int>(RES_DESTROY_POLE), 1 << 20};
    for (int h : heights) {
        const CAmount need = ResMinBurnForAtoms(RES_COIN, h);
        BOOST_CHECK_MESSAGE(CheckResMintArithmetic(RES_COIN, need, h),
                            "exact ceil must buy one RES at height " << h);
        BOOST_CHECK_MESSAGE(!CheckResMintArithmetic(RES_COIN, need - 1, h),
                            "one atom short must fail at height " << h);
        const CAmount shown = ResDestroyCostAtHeight(h);
        BOOST_CHECK_MESSAGE(shown == need || shown + 1 == need,
                            "display destroy cost must sit on or one atom below the ceil at " << h);
    }
}

BOOST_AUTO_TEST_CASE(no_free_mint_anywhere)
{
    const int heights[] = {0, 1, static_cast<int>(RES_DESTROY_POLE), 1 << 20,
                           std::numeric_limits<int>::max()};
    for (int h : heights) {
        BOOST_CHECK(!CheckResMintArithmetic(1, 0, h));
        BOOST_CHECK(ResDestroyCostAtHeight(h) > 0);
        BOOST_CHECK_GE(ResMinMintBurnAtoms(h), 1);
        BOOST_CHECK(ResMintAuthorizedAtHeight(h));
    }
}

BOOST_AUTO_TEST_CASE(negative_and_zero_inputs_are_refused_not_wrapped)
{
    BOOST_CHECK(!CheckResMintArithmetic(-1, MAX_MONEY, 0));
    BOOST_CHECK(!CheckResMintArithmetic(RES_COIN, -1, 0));
    BOOST_CHECK(!CheckResMintArithmetic(-1, -1, 0));
    BOOST_CHECK(!CheckResMintArithmetic(RES_COIN, MAX_MONEY, -1));
    BOOST_CHECK(CheckResMintArithmetic(0, MAX_MONEY, 0));
    BOOST_CHECK(CheckResMintArithmetic(0, 0, 0));
    BOOST_CHECK(CheckResMintArithmetic(0, MAX_MONEY, std::numeric_limits<int>::max()));
}

BOOST_AUTO_TEST_CASE(mints_above_the_derived_cap_are_refused)
{
    const CAmount cap = ResDerivedMaxSupply();
    BOOST_CHECK(!CheckResMintArithmetic(cap + 1, MAX_MONEY, 0));
    BOOST_CHECK(!CheckResMintArithmetic(MAX_MONEY, MAX_MONEY, 0));
    BOOST_CHECK(CheckResMintArithmetic(cap, MAX_MONEY, 0));
}

BOOST_AUTO_TEST_CASE(closed_form_cap_and_prefix)
{
    BOOST_CHECK_EQUAL(ResDerivedMaxSupply(), RES_MAX_SUPPLY);
    BOOST_CHECK_EQUAL(ResDerivedMaxSupply(), (static_cast<CAmount>(1) << 22) * RES_COIN);
    BOOST_CHECK_EQUAL(ResCumulativeMaxAtHeight(0), 0);
    BOOST_CHECK_LT(ResCumulativeMaxAtHeight(1), ResDerivedMaxSupply());
    BOOST_CHECK_LT(ResCumulativeMaxAtHeight(1 << 20), ResDerivedMaxSupply());
    BOOST_CHECK_LT(ResCumulativeMaxAtHeight(1), ResCumulativeMaxAtHeight(2));
}

BOOST_AUTO_TEST_CASE(wallet_inverse_maximises_the_predicate)
{
    const int heights[] = {0, 1, 2, 210000, static_cast<int>(RES_DESTROY_POLE)};
    for (int h : heights) {
        const CAmount need = ResMinBurnForAtoms(RES_COIN, h);
        const CAmount exact = ResMintAtomsFromBurn(need, h);
        BOOST_CHECK_EQUAL(exact, RES_COIN);
        BOOST_CHECK(CheckResMintArithmetic(exact, need, h));
        BOOST_CHECK(!CheckResMintArithmetic(exact + 1, need, h));

        const CAmount half_burn = need / 2;
        const CAmount half = ResMintAtomsFromBurn(half_burn, h);
        BOOST_CHECK(CheckResMintArithmetic(half, half_burn, h));
        if (half > 0) {
            BOOST_CHECK(!CheckResMintArithmetic(half + 1, half_burn, h));
        }
    }
    BOOST_CHECK_EQUAL(ResMintAtomsFromBurn(0, 0), 0);
    BOOST_CHECK_EQUAL(ResMintAtomsFromBurn(1, 0), 0);
    BOOST_CHECK_EQUAL(ResMintAtomsFromBurn(MAX_MONEY, -1), 0);
}

BOOST_AUTO_TEST_CASE(one_res_atom_floor_tracks_the_predicate)
{
    const int heights[] = {0, 1, 2, 210000, 420000, static_cast<int>(RES_DESTROY_POLE)};
    for (int h : heights) {
        const CAmount floor = ResMinMintBurnAtoms(h);
        BOOST_CHECK(floor > 0);
        BOOST_CHECK(CheckResMintArithmetic(1, floor, h));
        BOOST_CHECK(!CheckResMintArithmetic(1, floor - 1, h));
        BOOST_CHECK_EQUAL(ResMintAtomsFromBurn(floor, h), 1);
    }
    BOOST_CHECK_EQUAL(ResMinMintBurnAtoms(0), 1024);
    BOOST_CHECK_EQUAL(ResMinMintBurnAtoms(1), 1025);
    BOOST_CHECK_EQUAL(ResMinMintBurnAtoms(-1), 0);
}

BOOST_AUTO_TEST_CASE(ssot_probe_heights)
{
    BOOST_CHECK_EQUAL(ResDestroyCostAtHeight(0), RES_DESTROY_COST_GENESIS);
    BOOST_CHECK_EQUAL(ResDestroyCostAtHeight(1), 102400006103LL);
    BOOST_CHECK_EQUAL(ResDestroyCostAtHeight(209999), 103685743011LL);
    BOOST_CHECK_EQUAL(ResDestroyCostAtHeight(210000), 103685749152LL);
    BOOST_CHECK_EQUAL(ResDestroyCostAtHeight(static_cast<int>(RES_DESTROY_POLE)), 409600000000LL);
}

BOOST_AUTO_TEST_SUITE_END()
