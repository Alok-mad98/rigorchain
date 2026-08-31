// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-041 canonical issuance supply. Production consensus is not modified.
// Uses TestingSetup (mainnet interval 210000), not TestChain100Setup/regtest.

#include <test/data/issuance_height_boundary_p0_040.json.h>

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace {

CAmount CanonicalOruClosed(int64_t H)
{
    if (H <= 0) return 0;
    const CAmount r0 = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    const CAmount r1 = r0 >> 1;
    const CAmount r2 = r0 >> 2;
    const CAmount r3 = r0 >> 3;
    if (H < 210000) return H * r0;
    if (H < 420000) return 209999 * r0 + (H - 209999) * r1;
    if (H < 630000) return 209999 * r0 + 210000 * r1 + (H - 419999) * r2;
    return 209999 * r0 + 210000 * r1 + 210000 * r2 + (H - 629999) * r3;
}

CAmount CanonicalResClosed(int64_t H)
{
    if (H <= 0) return 0;
    const CAmount w0 = Consensus::RES_REWARD_GENESIS;
    const CAmount w1 = Consensus::ResRewardForEpoch(1);
    const CAmount w2 = Consensus::ResRewardForEpoch(2);
    const CAmount w3 = Consensus::ResRewardForEpoch(3);
    if (H < 210000) return H * w0;
    if (H < 420000) return 209999 * w0 + (H - 209999) * w1;
    if (H < 630000) return 209999 * w0 + 210000 * w1 + (H - 419999) * w2;
    if (H < 840000) return 209999 * w0 + 210000 * w1 + 210000 * w2 + (H - 629999) * w3;
    return -1;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(issuance_height_boundary_p0_041_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(genesis_reward_is_zero)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, c), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, c), Consensus::ORU_GENESIS_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(height_1_has_full_reward)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_NE(GetBlockSubsidy(1, c), 2 * Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(height_209999_is_last_full_epoch_block)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(209999 / Consensus::ORU_SUBSIDY_HALVING_INTERVAL, 0);
}

BOOST_AUTO_TEST_CASE(height_210000_is_first_halved_block)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(210000 / Consensus::ORU_SUBSIDY_HALVING_INTERVAL, 1);
    BOOST_CHECK_NE(GetBlockSubsidy(210000, c), GetBlockSubsidy(209999, c));
}

BOOST_AUTO_TEST_CASE(height_210001_is_second_halved_block)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210001, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210001, c), GetBlockSubsidy(210000, c));
    BOOST_CHECK_EQUAL(210001 / Consensus::ORU_SUBSIDY_HALVING_INTERVAL, 1);
}

BOOST_AUTO_TEST_CASE(height_420000_is_first_epoch_2_block)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);
    BOOST_CHECK_EQUAL(420000 / Consensus::ORU_SUBSIDY_HALVING_INTERVAL, 2);
}

BOOST_AUTO_TEST_CASE(oru_res_halving_boundary_matches)
{
    const int heights[] = {0, 1, 209998, 209999, 210000, 210001, 419999, 420000, 420001};
    const auto& c = Params().GetConsensus();
    for (int h : heights) {
        const int epoch = h / Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        BOOST_CHECK_EQUAL(epoch, h / Consensus::RES_REWARD_EPOCH);
        BOOST_CHECK_EQUAL(Consensus::GetResMintReward(h), Consensus::ResRewardForEpoch(epoch));
        if (h == 0) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), 0);
        } else {
            const int shifts = std::min(epoch, Consensus::ORU_SUBSIDY_MAX_SHIFTS);
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> shifts);
        }
    }
}

BOOST_AUTO_TEST_CASE(genesis_not_counted_as_issuance)
{
    BOOST_CHECK_EQUAL(CanonicalOruClosed(0), 0);
    BOOST_CHECK_EQUAL(CanonicalResClosed(0), 0);
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(0, Params().GetConsensus()), 0);
}

BOOST_AUTO_TEST_CASE(no_genesis_reward_recovery_at_height_1)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(1), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(cumulative_oru_first_epoch_and_halving)
{
    BOOST_CHECK_EQUAL(CanonicalOruClosed(209999), 20999900000000000LL);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(209999) / COIN, 209999000);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(210000), 20999950000000000LL);
    BOOST_CHECK_EQUAL(CanonicalOruClosed(210000) / COIN, 209999500);
    BOOST_CHECK_NE(CanonicalOruClosed(210000) / COIN, 210000000);
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(209999, c), CanonicalOruClosed(209999));
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(210000, c), CanonicalOruClosed(210000));
    BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(210001, c), CanonicalOruClosed(210001));
}

BOOST_AUTO_TEST_CASE(cumulative_res_first_epoch_and_halving)
{
    BOOST_CHECK_EQUAL(CanonicalResClosed(209999), 209999LL * Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(CanonicalResClosed(210000),
                      209999LL * Consensus::RES_REWARD_GENESIS + Consensus::ResRewardForEpoch(1));
    BOOST_CHECK_EQUAL(CanonicalResClosed(210001),
                      209999LL * Consensus::RES_REWARD_GENESIS + 2 * Consensus::ResRewardForEpoch(1));
}

BOOST_AUTO_TEST_CASE(closed_form_matches_per_block_increment)
{
    const auto& c = Params().GetConsensus();
    const int heights[] = {1, 2, 209998, 209999, 210000, 210001, 419999, 420000, 420001, 629999, 630000, 630001};
    for (int h : heights) {
        BOOST_CHECK_EQUAL(CanonicalOruClosed(h) - CanonicalOruClosed(h - 1), GetBlockSubsidy(h, c));
        BOOST_CHECK_EQUAL(CanonicalResClosed(h) - CanonicalResClosed(h - 1), Consensus::GetResMintReward(h));
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
        const int epoch = h / Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
        BOOST_CHECK_EQUAL(epoch, v["epoch"].getInt<int>());
        BOOST_CHECK_EQUAL(epoch, v["res_epoch"].getInt<int>());
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), v["oru_reward"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(Consensus::GetResMintReward(h), v["res_reward"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(CanonicalOruClosed(h), v["cumulative_oru"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(CanonicalResClosed(h), v["cumulative_res"].getInt<int64_t>());
        BOOST_CHECK_EQUAL(ComputeDerivedOruSubsidyCumulative(h, c), v["cumulative_oru"].getInt<int64_t>());
    }
}

BOOST_AUTO_TEST_SUITE_END()
