// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_global_candidate_equivalence.h>
#include <node/supply_monitor/runtime_supply_witness_equivalence_candidate.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <algorithm>
#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_global_candidate_equivalence_tests)

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

bool HasReason(const Consensus::GlobalCandidateEquivalenceObservation& g, const std::string& r)
{
    return std::find(g.blocking_reasons.begin(), g.blocking_reasons.end(), r) !=
           g.blocking_reasons.end();
}

struct GlobalFixture : BasicTestingSetup {
    GlobalFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~GlobalFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(global_comparable_with_parent_delta, GlobalFixture)
{
    const uint256 h0 = HashFromLabel("g-h0");
    const uint256 h1 = HashFromLabel("g-h1");
    const Consensus::RuntimeSupplyWitness w0 = MakeWitness(0, h0, uint256{}, 50 * COIN, 0);
    const Consensus::RuntimeSupplyWitness w1 = MakeWitness(1, h1, h0, 51 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w1, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             w0);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w1, catalog, chas);
    const Consensus::GlobalCandidateEquivalenceObservation global =
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate);

    BOOST_CHECK_EQUAL(global.oru_global_status,
                      Consensus::CandidateEquivalenceStatus::PartiallyComparable);
    BOOST_CHECK_EQUAL(global.res_global_status,
                      Consensus::CandidateEquivalenceStatus::PartiallyComparable);
    BOOST_CHECK(!global.equivalence_claimed);
    BOOST_CHECK(HasReason(global, "GLOBAL-CHAS-CANDIDATE-AGGREGATE"));
}

BOOST_FIXTURE_TEST_CASE(global_genesis_partial_oru, GlobalFixture)
{
    const Consensus::RuntimeSupplyWitness w0 =
        MakeWitness(0, HashFromLabel("g-gen"), uint256{}, 50 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w0, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w0, catalog, chas);
    const Consensus::GlobalCandidateEquivalenceObservation global =
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate);

    BOOST_CHECK_EQUAL(global.oru_global_status,
                      Consensus::CandidateEquivalenceStatus::PartiallyComparable);
}

BOOST_FIXTURE_TEST_CASE(global_broken_chain_blocked, GlobalFixture)
{
    const Consensus::RuntimeSupplyWitness parent =
        MakeWitness(4, HashFromLabel("g-p4"), HashFromLabel("g-p3"), 10 * COIN, 0);
    const Consensus::RuntimeSupplyWitness orphan =
        MakeWitness(5, HashFromLabel("g-bad"), HashFromLabel("wrong"), 99 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(orphan, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             parent);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(orphan, catalog, chas);
    const Consensus::GlobalCandidateEquivalenceObservation global =
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate);

    BOOST_CHECK_EQUAL(global.oru_global_status,
                      Consensus::CandidateEquivalenceStatus::FundamentallyBlocked);
    BOOST_CHECK(HasReason(global, "WITNESS-CHAIN-CES-FundamentallyBlocked"));
}

BOOST_FIXTURE_TEST_CASE(global_draw_blocked_in_reasons, GlobalFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(2, HashFromLabel("g-d2"), HashFromLabel("g-d1"), 5 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w, catalog, chas);
    const Consensus::GlobalCandidateEquivalenceObservation global =
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate);

    BOOST_CHECK(HasReason(global, "DRAW-CES-FundamentallyBlocked"));
}

BOOST_FIXTURE_TEST_CASE(global_missing_witness_returns_nullopt, GlobalFixture)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 88;
    BOOST_CHECK(!Consensus::ResolveGlobalCandidateEquivalenceAt(&pindex).has_value());
}

BOOST_FIXTURE_TEST_CASE(global_export_json_coq_field_names, GlobalFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(0, HashFromLabel("g-exp"), uint256{}, 50 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w, catalog, chas);
    const Consensus::GlobalCandidateEquivalenceExport exp{
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate), chas, candidate};
    const UniValue json = Consensus::GlobalCandidateEquivalenceExportToJson(exp);

    BOOST_CHECK_EQUAL(json["task"].get_str(), "FVR-K");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK(json["GlobalCandidateEquivalenceObservation"].isObject());
    BOOST_CHECK_EQUAL(json["GlobalCandidateEquivalenceObservation"]["gceo_oru_global_status"].get_str(),
                      "CES_PartiallyComparable");
    BOOST_CHECK(json["chas_report"].isObject());
    BOOST_CHECK(json["runtime_candidate"].isObject());
}

BOOST_AUTO_TEST_SUITE_END()
