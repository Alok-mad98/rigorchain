// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// RFC-020 flat ORU subsidy + continuous RES display destroy cost.
// Uses TestingSetup (mainnet observational interval 210000).

#include <test/data/issuance_height_boundary_p0_040.json.h>

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>

namespace {

CAmount CanonicalOruClosed(int64_t H)
{
    if (H <= 0) return 0;
    return H * Consensus::ORU_FLAT_BLOCK_SUBSIDY;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(issuance_height_boundary_p0_041_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(genesis_reward_is_zero)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, c), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, c), Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(height_1_has_flat_reward)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c), Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_NE(GetBlockSubsidy(1, c), 2 * Consensus::ORU_FLAT_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(flat_subsidy_does_not_halve)
{
    const auto& c = Params().GetConsensus();
    const int heights[] = {1, 2, 209998, 209999, 210000, 210001, 419999, 420000, 420001,
                           629999, 630000, 630001};
    for (int h : heights) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(-1, c), 0);
}

BOOST_AUTO_TEST_CASE(genesis_not_counted_as_issuance)
{
    BOOST_CHECK_EQUAL(CanonicalOruClosed(0), 0);
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(0, Params().GetConsensus()), 0);
}

BOOST_AUTO_TEST_CASE(no_genesis_reward_recovery_at_height_1)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c), Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(1), Consensus::ORU_FLAT_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(cumulative_oru_is_height_times_flat)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(CanonicalOruClosed(209999), 209999 * Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(210000), 210000 * Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(209999, c), CanonicalOruClosed(209999));
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(210000, c), CanonicalOruClosed(210000));
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(210001, c), CanonicalOruClosed(210001));
}

BOOST_AUTO_TEST_CASE(closed_form_matches_per_block_increment)
{
    const auto& c = Params().GetConsensus();
    const int heights[] = {1, 2, 209998, 209999, 210000, 210001, 419999, 420000, 420001, 629999, 630000, 630001};
    for (int h : heights) {
        BOOST_CHECK_EQUAL(CanonicalOruClosed(h) - CanonicalOruClosed(h - 1), GetBlockSubsidy(h, c));
    }
}

BOOST_AUTO_TEST_CASE(canonical_golden_vectors)
{
    UniValue root;
    const std::string raw(
        json_tests::issuance_height_boundary_p0_040,
        json_tests::issuance_height_boundary_p0_040 + sizeof(json_tests::issuance_height_boundary_p0_040));
    BOOST_REQUIRE(root.read(raw));
    const auto& c = Params().GetConsensus();
    const UniValue& vectors = root["vectors"];
    for (size_t i = 0; i < vectors.size(); ++i) {
        const UniValue& v = vectors[i];
        const int h = v["height"].getInt<int>();
        const int epoch = h / Consensus::RES_REWARD_EPOCH;
        BOOST_CHECK_EQUAL(epoch, v["epoch"].getInt<int>());
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), v["oru_reward"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(Consensus::ResDestroyCostAtHeight(h), v["res_destroy_cost"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(CanonicalOruClosed(h), v["cumulative_oru"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(h, c), v["cumulative_oru"].getInt<int64_t>());
    }
}

BOOST_AUTO_TEST_SUITE_END()
