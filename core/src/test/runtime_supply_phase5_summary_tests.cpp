// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_phase5_summary.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_phase5_summary_tests)

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
                                            CAmount res,
                                            bool passed = true)
{
    Consensus::RuntimeSupplyWitness w;
    w.height = height;
    w.block_hash = block_hash;
    w.parent_hash = parent_hash;
    w.oru_supply_cumulative = oru;
    w.res_mint_cumulative = res;
    w.invariants_passed = passed;
    return w;
}

const Consensus::Phase5LayerSummary* FindLayer(const Consensus::Phase5SummaryExport& exp,
                                             const std::string& name)
{
    for (const Consensus::Phase5LayerSummary& layer : exp.layers) {
        if (layer.layer_name == name) {
            return &layer;
        }
    }
    return nullptr;
}

struct SummaryFixture : BasicTestingSetup {
    SummaryFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~SummaryFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(phase5_summary_json_shape, SummaryFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(0, HashFromLabel("p5-gen"), uint256{}, 50 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 0;
    pindex.phashBlock = &w.block_hash;

    const std::optional<Consensus::Phase5SummaryExport> exp =
        Consensus::ResolvePhase5SummaryAt(&pindex);
    BOOST_REQUIRE(exp.has_value());

    const UniValue json = Consensus::Phase5SummaryExportToJson(*exp);
    BOOST_CHECK_EQUAL(json["task"].get_str(), "FVR-N");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    BOOST_CHECK_EQUAL(json["pd_4f_ob_02"].get_str(), "OPEN");
    BOOST_CHECK(json["phase5_layer_catalog"].isArray());
    BOOST_CHECK_EQUAL(json["phase5_layer_catalog"].size(), 8U);
    BOOST_CHECK(json["freeze_protocol_link"].isStr());
}

BOOST_FIXTURE_TEST_CASE(phase5_layer_statuses_with_witness, SummaryFixture)
{
    const uint256 h0 = HashFromLabel("p5-h0");
    const uint256 h1 = HashFromLabel("p5-h1");
    Consensus::StoreRuntimeSupplyWitness(
        MakeWitness(0, h0, uint256{}, 50 * COIN, 0));
    Consensus::StoreRuntimeSupplyWitness(
        MakeWitness(1, h1, h0, 51 * COIN, 0));

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 1;
    pindex.phashBlock = &h1;

    const std::optional<Consensus::Phase5SummaryExport> exp =
        Consensus::ResolvePhase5SummaryAt(&pindex);
    BOOST_REQUIRE(exp.has_value());

    const Consensus::Phase5LayerSummary* supply = FindLayer(*exp, "RuntimeSupply");
    const Consensus::Phase5LayerSummary* witness = FindLayer(*exp, "RuntimeWitnessChain");
    BOOST_REQUIRE(supply);
    BOOST_REQUIRE(witness);

    BOOST_CHECK(supply->runtime_present);
    BOOST_CHECK(witness->runtime_present);
}

BOOST_FIXTURE_TEST_CASE(phase5_summary_missing_witness_partial_layers, SummaryFixture)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 99;

    const std::optional<Consensus::Phase5SummaryExport> exp =
        Consensus::ResolvePhase5SummaryAt(&pindex);
    BOOST_REQUIRE(exp.has_value());

    const Consensus::Phase5LayerSummary* catalog = FindLayer(*exp, "RuntimeCoqCatalog");
    BOOST_REQUIRE(catalog);
    BOOST_CHECK_EQUAL(catalog->status, Consensus::Phase5LayerStatus::Partial);
    BOOST_CHECK(!catalog->runtime_present);
}

BOOST_FIXTURE_TEST_CASE(phase5_final_summary_json_shape, SummaryFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(0, HashFromLabel("p5f-gen"), uint256{}, 50 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 0;
    pindex.phashBlock = &w.block_hash;

    const std::optional<Consensus::Phase5SummaryFinalExport> exp =
        Consensus::ResolvePhase5SummaryFinalAt(&pindex);
    BOOST_REQUIRE(exp.has_value());

    const UniValue json = Consensus::Phase5SummaryFinalExportToJson(*exp);
    BOOST_CHECK_EQUAL(json["task"].get_str(), "PROOF-027");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["pd_4f_ob_02"].get_str(), "READY");
    BOOST_CHECK_EQUAL(json["pd_4f_cpp_01"].get_str(), "OPEN");
    BOOST_CHECK_EQUAL(json["pd13_status"].get_str(), "CLOSED");
    BOOST_CHECK_EQUAL(json["candidate_equivalence_runtime_ready"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["cpp01_candidate_equivalence_ready"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["golden_vector_total"].getInt<int>(), 12);
    BOOST_CHECK_EQUAL(json["golden_vector_materialized"].getInt<int>(), 9);
    BOOST_CHECK_EQUAL(json["ecs_prohibited_count"].getInt<int>(), 2);
    BOOST_CHECK(json["phase5_layer_catalog"].isArray());
}

BOOST_FIXTURE_TEST_CASE(phase5_final_consistency_alignment, SummaryFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(0, HashFromLabel("p5c-gen"), uint256{}, 50 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 0;
    pindex.phashBlock = &w.block_hash;

    const std::optional<Consensus::Phase5SummaryFinalExport> exp =
        Consensus::ResolvePhase5SummaryFinalAt(&pindex);
    BOOST_REQUIRE(exp.has_value());

    const UniValue json = Consensus::Phase5SummaryFinalExportToJson(*exp);
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    // RFC-019: draw observer retired; field must stay absent (not null-as-bool).
    BOOST_CHECK(!json.exists("draw_permanently_non_closable"));
    BOOST_CHECK(!json.exists("draw_status"));
    BOOST_CHECK(json["phase5_final_catalog_established"].get_bool());
    BOOST_CHECK_EQUAL(
        json["ecs_open_count"].getInt<int>() +
            json["ecs_partial_count"].getInt<int>() +
            json["ecs_prohibited_count"].getInt<int>(),
        json["golden_vector_total"].getInt<int>());
    BOOST_CHECK_EQUAL(json["freeze_protocol_link"].get_str(),
 "MAINNET_PROTOCOL_FREEZE_DECLARATION.md/");
}

BOOST_AUTO_TEST_SUITE_END()
