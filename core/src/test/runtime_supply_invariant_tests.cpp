// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_invariant_checker.h>

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
    s.res_mint_present_block = 0;
    s.res_mint_cumulative = 0;
    s.res_max_supply = Consensus::RES_MAX_SUPPLY;
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
    const CAmount res_mint = 1 * COIN;  // Synthetic RES mint for testing
    s.res_mint_parent = 0;
    s.res_mint_block = res_mint;
    s.res_mint_present_block = 1;
    s.res_mint_cumulative = res_mint;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(r.passed);
    BOOST_CHECK(r.violations.empty());
}

// RFC-019 removed the one-mint-per-block cap that the lottery imposed, so M(h) is an
// indicator rather than a cardinality. A block carrying many mints must still verify:
// the amounts sum into res_mint_block and M(h) stays 1. This is the case that the old
// RES-L4 reading ("M(h) counts the winner") would have rejected outright.
BOOST_AUTO_TEST_CASE(many_mints_in_one_block_pass)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    const CAmount total = 7 * COIN;  // e.g. seven independent minters at the epoch price
    s.res_mint_parent = 0;
    s.res_mint_block = total;
    s.res_mint_present_block = 1;
    s.res_mint_cumulative = total;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(r.passed);
    BOOST_CHECK(r.violations.empty());
}

// The inflation-facing direction of RES-L6: RES may not appear without a mint.
BOOST_AUTO_TEST_CASE(mint_amount_without_presence_fails_res_l6)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.res_mint_block = 5 * COIN;
    s.res_mint_present_block = 0;
    s.res_mint_cumulative = 5 * COIN;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_REQUIRE_EQUAL(r.violations.size(), 1U);
    BOOST_CHECK_EQUAL(r.violations[0], "RES-L6");
}

// And the converse: a mint flag with no amount behind it is equally incoherent.
BOOST_AUTO_TEST_CASE(presence_without_mint_amount_fails_res_l6)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s = ValidOruResState(consensus);
    s.res_mint_block = 0;
    s.res_mint_present_block = 1;
    s.res_mint_cumulative = 0;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_REQUIRE_EQUAL(r.violations.size(), 1U);
    BOOST_CHECK_EQUAL(r.violations[0], "RES-L6");
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
    s.res_mint_present_block = 0;
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
    const CAmount res_mint = 1 * COIN;  // Synthetic RES mint for testing
    s.res_mint_block = res_mint;
    s.res_mint_present_block = 0;  // Mismatch: non-zero mint with zero count
    s.res_mint_cumulative = res_mint;
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
    s.res_mint_present_block = 2;
    s.res_mint_cumulative = Consensus::RES_MAX_SUPPLY + 100;
    const Consensus::RuntimeInvariantResult r = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    BOOST_CHECK(!r.passed);
    BOOST_CHECK(r.violations.size() >= 4U);
    // Pinned to the constant, not a literal: the count moved 10 -> 8 -> 9 across the
    // RFC-019 refactor and a hardcoded number just goes stale silently.
    BOOST_CHECK_EQUAL(r.checked_invariants, Consensus::RUNTIME_SUPPLY_INVARIANT_COUNT);
}

BOOST_AUTO_TEST_SUITE_END()
