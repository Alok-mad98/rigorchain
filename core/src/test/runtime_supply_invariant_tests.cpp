// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_invariant_checker.h>

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <validation.h>

BOOST_FIXTURE_TEST_SUITE(runtime_supply_invariant_tests, BasicTestingSetup)

namespace {

Consensus::RuntimeSupplyState ValidOruResState(const Consensus::Params& consensus)
{
    Consensus::RuntimeSupplyState s;
    s.height = 5;
    s.oru_supply_parent = 100 * COIN;
    s.oru_subsidy_block = GetBlockSubsidy(5, consensus);
    s.oru_converge_burn_block = COIN;
    s.oru_supply_cumulative = s.oru_supply_parent + s.oru_subsidy_block - s.oru_converge_burn_block;
    s.res_mint_parent = 0;
    s.res_mint_block = 0;
    s.res_mint_count_block = 0;
    s.res_mint_cumulative = 0;
    s.res_max_supply = Consensus::RES_MAX_SUPPLY;
    s.res_reward_amount = Consensus::RES_REWARD_GENESIS;
    return s;
}

} // namespace

BOOST_AUTO_TEST_CASE(valid_oru_recurrence_passes)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    const Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(r.passed);
    BOOST_CHECK_EQUAL(r.checked_invariants, Consensus::RUNTIME_SUPPLY_INVARIANT_COUNT);
    BOOST_CHECK(r.violations.empty());
}

BOOST_AUTO_TEST_CASE(valid_res_recurrence_with_mint_passes)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.res_mint_parent = 0;
    s.res_mint_block = Consensus::RES_REWARD_GENESIS;
    s.res_mint_count_block = 1;
    s.res_mint_cumulative = Consensus::RES_REWARD_GENESIS;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(r.passed);
    BOOST_CHECK(r.violations.empty());
}

BOOST_AUTO_TEST_CASE(negative_oru_supply_fails_oru_l1)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.oru_supply_cumulative = -1;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_REQUIRE_EQUAL(r.violations.size(), 2U);
    BOOST_CHECK_EQUAL(r.violations[0], "ORU-L1");
    BOOST_CHECK_EQUAL(r.violations[1], "ORU-L3");
}

BOOST_AUTO_TEST_CASE(res_cap_violation_fails_res_l2)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.res_mint_parent = Consensus::RES_MAX_SUPPLY + 1;
    s.res_mint_block = 0;
    s.res_mint_count_block = 0;
    s.res_mint_cumulative = s.res_mint_parent + s.res_mint_block;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_REQUIRE_EQUAL(r.violations.size(), 1U);
    BOOST_CHECK_EQUAL(r.violations[0], "RES-L2");
}

BOOST_AUTO_TEST_CASE(res_mint_mismatch_fails_res_l6)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.res_mint_block = Consensus::RES_REWARD_GENESIS;
    s.res_mint_count_block = 0;
    s.res_mint_cumulative = Consensus::RES_REWARD_GENESIS;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_REQUIRE_EQUAL(r.violations.size(), 1U);
    BOOST_CHECK_EQUAL(r.violations[0], "RES-L6");
}

BOOST_AUTO_TEST_CASE(multiple_simultaneous_violations)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.oru_supply_cumulative = -1;
    s.oru_converge_burn_block = -1;
    s.res_mint_block = 1;
    s.res_mint_count_block = 2;
    s.res_mint_cumulative = Consensus::RES_MAX_SUPPLY + 100;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_CHECK(r.violations.size() >= 4U);
    BOOST_CHECK_EQUAL(r.checked_invariants, 10U);
}

BOOST_AUTO_TEST_SUITE_END()
