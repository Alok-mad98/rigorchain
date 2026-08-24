// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_witness_coq_adapter.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_witness_coq_adapter_tests)

namespace {

uint256 HashFromLabel(const char* label)
{
    uint256 h;
    std::memcpy(h.begin(), label, std::min(strlen(label), size_t{32}));
    return h;
}

Consensus::RuntimeSupplyWitness MakeWitness(int32_t height,
                                            const uint256& block_hash,
                                            const uint256& parent_hash,
                                            CAmount oru,
                                            CAmount res)
{
    Consensus::RuntimeSupplyWitness w;
    w.height = height;
    w.block_hash = block_hash;
    w.parent_hash = parent_hash;
    w.oru_supply_cumulative = oru;
    w.res_mint_cumulative = res;
    return w;
}

Consensus::RuntimeSupplyState MakeMirror(int32_t height,
                                         const uint256& block_hash,
                                         const uint256& parent_hash,
                                         CAmount oru_parent,
                                         CAmount oru_cum,
                                         CAmount subsidy,
                                         CAmount burn,
                                         CAmount res_parent,
                                         CAmount res_block,
                                         uint8_t mint_count)
{
    Consensus::RuntimeSupplyState m;
    m.height = height;
    m.block_hash = block_hash;
    m.parent_hash = parent_hash;
    m.oru_supply_parent = oru_parent;
    m.oru_subsidy_block = subsidy;
    m.oru_converge_burn_block = burn;
    m.oru_supply_cumulative = oru_cum;
    m.res_mint_parent = res_parent;
    m.res_mint_block = res_block;
    m.res_mint_count_block = mint_count;
    m.res_mint_cumulative = res_parent + res_block;
    m.res_max_supply = Consensus::RES_MAX_SUPPLY;
    m.res_reward_amount = Consensus::RES_REWARD_GENESIS;
    return m;
}

struct WitnessCoqFixture : BasicTestingSetup {
    WitnessCoqFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~WitnessCoqFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(build_catalog_row_from_witness_chain, WitnessCoqFixture)
{
    const uint256 h0 = HashFromLabel("genesis-block");
    const uint256 h1 = HashFromLabel("block-one");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(0, h0, uint256{}, 50 * COIN, 0));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1, h1, h0, 50 * COIN + COIN, Consensus::RES_REWARD_GENESIS));

    const std::optional<Consensus::RuntimeSupplyWitness> w1 = Consensus::GetRuntimeSupplyWitness(h1);
    BOOST_REQUIRE(w1.has_value());
    const std::optional<Consensus::RuntimeSupplyWitness> w0 = Consensus::GetRuntimeSupplyWitness(h0);
    BOOST_REQUIRE(w0.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w1, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, w0);

    BOOST_CHECK_EQUAL(row.height, 1);
    BOOST_CHECK_EQUAL(row.rs_oru_supply, w1->oru_supply_cumulative);
    BOOST_CHECK_EQUAL(row.rs_res_supply, w1->res_mint_cumulative);
    BOOST_CHECK(row.delta.parent_witness_present);
    BOOST_CHECK(row.delta.witness_chain_step_valid);
    BOOST_CHECK_EQUAL(row.rs_oru_supply, row.coq_hook.rs_oru_supply);
    BOOST_CHECK(!row.equivalence_claimed);
}

BOOST_FIXTURE_TEST_CASE(parent_child_delta_matches_hook, WitnessCoqFixture)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    const uint256 parent_hash = HashFromLabel("parent-9");
    const uint256 child_hash = HashFromLabel("child-10");
    const CAmount subsidy = GetBlockSubsidy(10, consensus);
    const CAmount burn = COIN;
    const CAmount parent_oru = 100 * COIN;
    const CAmount child_oru = parent_oru + subsidy - burn;

    const Consensus::RuntimeSupplyWitness parent =
        MakeWitness(9, parent_hash, HashFromLabel("parent-8"), parent_oru, 0);
    const Consensus::RuntimeSupplyWitness child =
        MakeWitness(10, child_hash, parent_hash, child_oru, Consensus::RES_REWARD_GENESIS);
    Consensus::StoreRuntimeSupplyWitness(parent);
    Consensus::StoreRuntimeSupplyWitness(child);

    const Consensus::RuntimeSupplyState mirror = MakeMirror(
        10, child_hash, parent_hash, parent_oru, child_oru, subsidy, burn, 0,
        Consensus::RES_REWARD_GENESIS, 1);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        child, mirror, true, Consensus::RuntimeSupplyMirrorSource::STORED_MIRROR, parent);

    BOOST_CHECK_EQUAL(row.delta.rs_oru_supply_delta, subsidy - burn);
    BOOST_CHECK_EQUAL(row.delta.rs_res_supply_delta, Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK(Consensus::CheckWitnessCoqDeltaMatchesHook(row).empty());
}

BOOST_FIXTURE_TEST_CASE(rpc_catalog_json_shape, WitnessCoqFixture)
{
    const uint256 h5 = HashFromLabel("block-5");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(5, h5, HashFromLabel("block-4"), 123456, 7800));
    const std::optional<Consensus::RuntimeSupplyWitness> w = Consensus::GetRuntimeSupplyWitness(h5);
    BOOST_REQUIRE(w.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, std::nullopt);
    const UniValue json = Consensus::WitnessCoqCatalogRowToJson(row);

    BOOST_CHECK_EQUAL(json["height"].getInt<int>(), 5);
    BOOST_CHECK_EQUAL(json["mirror_present"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["mirror_source"].get_str(), "witness_only");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    BOOST_CHECK(json["coq_catalog"].isObject());
    BOOST_CHECK_EQUAL(json["coq_catalog"]["rs_oru_supply"].getValStr(), "0.00123456");
    BOOST_CHECK(json["parent_delta"].isObject());
}

BOOST_FIXTURE_TEST_CASE(first_block_missing_parent, WitnessCoqFixture)
{
    const uint256 h0 = HashFromLabel("genesis");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(0, h0, uint256{}, 50 * COIN, 0));
    const std::optional<Consensus::RuntimeSupplyWitness> w0 = Consensus::GetRuntimeSupplyWitness(h0);
    BOOST_REQUIRE(w0.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w0, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, std::nullopt);

    BOOST_CHECK(!row.delta.parent_witness_present);
    BOOST_CHECK(row.delta.witness_chain_step_valid);
    BOOST_CHECK_EQUAL(row.delta.rs_oru_supply_delta, 0);
}

BOOST_FIXTURE_TEST_CASE(reorg_drops_parent_witness, WitnessCoqFixture)
{
    const uint256 h4 = HashFromLabel("block-4");
    const uint256 h5 = HashFromLabel("block-5");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(4, h4, HashFromLabel("block-3"), 10 * COIN, 0));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(5, h5, h4, 11 * COIN, 0));
    Consensus::InvalidateRuntimeSupplyWitness(h5);

    const std::optional<Consensus::RuntimeSupplyWitness> w4 = Consensus::GetRuntimeSupplyWitness(h4);
    BOOST_REQUIRE(w4.has_value());
    const Consensus::RuntimeSupplyWitness orphan =
        MakeWitness(5, HashFromLabel("reorg-tip"), HashFromLabel("wrong-parent"), 99 * COIN, 0);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        orphan, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, w4);

    BOOST_CHECK(!row.delta.witness_chain_step_valid);
    const std::vector<std::string> audit = Consensus::CheckWitnessCoqDeltaMatchesHook(row);
    BOOST_REQUIRE_EQUAL(audit.size(), 1U);
    BOOST_CHECK_EQUAL(audit[0], "WITNESS-CHAIN-BROKEN");
}

BOOST_FIXTURE_TEST_CASE(resolve_from_pindex_mirror, WitnessCoqFixture)
{
    const uint256 tip_hash = HashFromLabel("resolve-tip");
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 3;
    pindex.phashBlock = &tip_hash;

    Consensus::RuntimeSupplyWitness witness =
        MakeWitness(3, tip_hash, HashFromLabel("resolve-parent"), 30 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(witness);

    const Consensus::RuntimeSupplyState mirror = MakeMirror(
        3, tip_hash, HashFromLabel("resolve-parent"), 29 * COIN, 30 * COIN, COIN, 0, 0, 0, 0);
    Consensus::StoreRuntimeSupplyMirror(&pindex, mirror);

    const std::optional<Consensus::RuntimeSupplyWitnessCoqCatalogRow> row =
        Consensus::ResolveWitnessCoqCatalogRowAt(&pindex);
    BOOST_REQUIRE(row.has_value());
    BOOST_CHECK(row->mirror_present);
    BOOST_CHECK_EQUAL(row->mirror_source, Consensus::RuntimeSupplyMirrorSource::STORED_MIRROR);
    BOOST_CHECK_EQUAL(row->coq_hook.cb_oru_subsidy, COIN);
}

BOOST_AUTO_TEST_SUITE_END()
