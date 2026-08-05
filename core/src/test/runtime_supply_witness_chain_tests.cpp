// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_invariant_checker.h>
#include <consensus/runtime_supply_witness_chain.h>

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <core_io.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <cstring>

BOOST_FIXTURE_TEST_SUITE(runtime_supply_witness_chain_tests, BasicTestingSetup)

namespace {

Consensus::RuntimeSupplyWitness MakeWitness(int32_t height,
                                            const uint256& block_hash,
                                            const uint256& parent_hash,
                                            bool passed = true)
{
    Consensus::RuntimeSupplyWitness w;
    w.height = height;
    w.block_hash = block_hash;
    w.parent_hash = parent_hash;
    w.invariants_passed = passed;
    w.checked_invariants = Consensus::RUNTIME_SUPPLY_INVARIANT_COUNT;
    w.oru_supply_cumulative = static_cast<CAmount>(height) * COIN;
    w.res_mint_cumulative = static_cast<CAmount>(height) * Consensus::RES_COIN;
    return w;
}

uint256 HashFromLabel(const char* label)
{
    uint256 h;
    std::memcpy(h.begin(), label, std::min(strlen(label), size_t{32}));
    return h;
}

UniValue WitnessToJsonForTest(const Consensus::RuntimeSupplyWitness& witness)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", witness.height);
    obj.pushKV("block_hash", witness.block_hash.GetHex());
    obj.pushKV("parent_hash", witness.parent_hash.IsNull() ? "" : witness.parent_hash.GetHex());
    obj.pushKV("invariants_passed", witness.invariants_passed);
    obj.pushKV("checked_invariants", static_cast<int>(witness.checked_invariants));
    UniValue violations(UniValue::VARR);
    for (const std::string& id : witness.violations) {
        violations.push_back(id);
    }
    obj.pushKV("violations", std::move(violations));
    obj.pushKV("oru_supply_cumulative", ValueFromAmount(witness.oru_supply_cumulative));
    obj.pushKV("res_mint_cumulative", ValueFromAmount(witness.res_mint_cumulative));
    return obj;
}

} // namespace

struct WitnessChainFixture {
    WitnessChainFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~WitnessChainFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

BOOST_FIXTURE_TEST_CASE(store_and_retrieve_witness, WitnessChainFixture)
{
    const Consensus::RuntimeSupplyWitness w = MakeWitness(5, HashFromLabel("witness-block-5"), HashFromLabel("witness-parent-4"));
    Consensus::StoreRuntimeSupplyWitness(w);
    const std::optional<Consensus::RuntimeSupplyWitness> got = Consensus::GetRuntimeSupplyWitness(5);
    BOOST_REQUIRE(got.has_value());
    BOOST_CHECK_EQUAL(got->height, 5);
    BOOST_CHECK(got->block_hash == w.block_hash);
    BOOST_CHECK(got->invariants_passed);
}

BOOST_FIXTURE_TEST_CASE(latest_witness_tracks_highest_height, WitnessChainFixture)
{
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1, HashFromLabel("w1"), uint256{}));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(3, HashFromLabel("w3"), HashFromLabel("w2")));
    const std::optional<Consensus::RuntimeSupplyWitness> latest = Consensus::GetLatestRuntimeSupplyWitness();
    BOOST_REQUIRE(latest.has_value());
    BOOST_CHECK_EQUAL(latest->height, 3);
}

BOOST_FIXTURE_TEST_CASE(parent_child_consistency_passes, WitnessChainFixture)
{
    const uint256 parent_hash = HashFromLabel("parent-at-9");
    const uint256 child_hash = HashFromLabel("child-at-10");
    const Consensus::RuntimeSupplyWitness parent = MakeWitness(9, parent_hash, HashFromLabel("parent-at-8"));
    const Consensus::RuntimeSupplyWitness child = MakeWitness(10, child_hash, parent_hash);
    BOOST_CHECK(Consensus::VerifyRuntimeWitnessChainStep(parent, child));
}

BOOST_FIXTURE_TEST_CASE(broken_chain_detection, WitnessChainFixture)
{
    const Consensus::RuntimeSupplyWitness parent = MakeWitness(7, HashFromLabel("p7"), HashFromLabel("p6"));
    Consensus::RuntimeSupplyWitness bad_height = MakeWitness(9, HashFromLabel("c9"), parent.block_hash);
    BOOST_CHECK(!Consensus::VerifyRuntimeWitnessChainStep(parent, bad_height));

    Consensus::RuntimeSupplyWitness bad_parent_link = MakeWitness(8, HashFromLabel("c8"), HashFromLabel("wrong-parent"));
    BOOST_CHECK(!Consensus::VerifyRuntimeWitnessChainStep(parent, bad_parent_link));
}

BOOST_AUTO_TEST_CASE(build_witness_from_mirror_and_invariants)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    Consensus::RuntimeSupplyState s;
    s.height = 2;
    s.block_hash = HashFromLabel("mirror-block-2");
    s.parent_hash = HashFromLabel("mirror-parent-1");
    s.oru_supply_parent = 50 * COIN;
    s.oru_subsidy_block = GetBlockSubsidy(2, consensus);
    s.oru_converge_burn_block = 0;
    s.oru_supply_cumulative = s.oru_supply_parent + s.oru_subsidy_block;
    s.res_mint_parent = 0;
    s.res_mint_block = 0;
    s.res_mint_count_block = 0;
    s.res_mint_cumulative = 0;
    s.res_max_supply = Consensus::RES_MAX_SUPPLY;
    s.res_reward_amount = Consensus::RES_REWARD_AMOUNT;

    const Consensus::RuntimeInvariantResult inv = Consensus::VerifyRuntimeSupplyInvariants(s, consensus);
    const Consensus::RuntimeSupplyWitness w = Consensus::BuildRuntimeSupplyWitness(s, inv);
    BOOST_CHECK(w.invariants_passed);
    BOOST_CHECK_EQUAL(w.height, 2);
    BOOST_CHECK_EQUAL(w.oru_supply_cumulative, s.oru_supply_cumulative);
    BOOST_CHECK_EQUAL(w.res_mint_cumulative, 0);
}

BOOST_FIXTURE_TEST_CASE(rpc_export_shape, WitnessChainFixture)
{
    Consensus::RuntimeSupplyWitness w = MakeWitness(1000, HashFromLabel("block-1000"), HashFromLabel("block-999"));
    w.oru_supply_cumulative = 123456;
    w.res_mint_cumulative = 7800;
    const UniValue json = WitnessToJsonForTest(w);
    BOOST_CHECK_EQUAL(json["height"].getInt<int>(), 1000);
    BOOST_CHECK_EQUAL(json["invariants_passed"].get_bool(), true);
    BOOST_CHECK_EQUAL(json["checked_invariants"].getInt<int>(), 10);
    BOOST_CHECK(json["violations"].isArray());
    BOOST_CHECK_EQUAL(json["violations"].size(), 0U);
    BOOST_CHECK_EQUAL(json["oru_supply_cumulative"].getValStr(), "0.00123456");
    BOOST_CHECK_EQUAL(json["res_mint_cumulative"].getValStr(), "0.00007800");
}

BOOST_FIXTURE_TEST_CASE(invalidate_drops_witness_and_updates_latest, WitnessChainFixture)
{
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(4, HashFromLabel("w4"), HashFromLabel("w3")));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(5, HashFromLabel("w5"), HashFromLabel("w4")));
    Consensus::InvalidateRuntimeSupplyWitness(5);
    BOOST_CHECK(!Consensus::GetRuntimeSupplyWitness(5).has_value());
    const std::optional<Consensus::RuntimeSupplyWitness> latest = Consensus::GetLatestRuntimeSupplyWitness();
    BOOST_REQUIRE(latest.has_value());
    BOOST_CHECK_EQUAL(latest->height, 4);
}

BOOST_AUTO_TEST_SUITE_END()
