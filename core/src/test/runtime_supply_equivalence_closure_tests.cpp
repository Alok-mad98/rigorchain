// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_equivalence_closure.h>
#include <consensus/runtime_supply_witness_equivalence_candidate.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_equivalence_closure_tests)

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

bool HasLayer(const std::vector<std::string>& layers, const std::string& name)
{
    return std::find(layers.begin(), layers.end(), name) != layers.end();
}

struct ClosureFixture : BasicTestingSetup {
    ClosureFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~ClosureFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(closure_missing_witness_chain_open, ClosureFixture)
{
    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

    BOOST_CHECK_EQUAL(result.status, Consensus::EquivalenceClosureStatus::Open);
    BOOST_CHECK(!result.closure_allowed);
    BOOST_CHECK(HasLayer(result.missing_layers, "FVR-F-runtime-witness-chain"));
}

BOOST_FIXTURE_TEST_CASE(closure_missing_invariant_layer, ClosureFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(7, HashFromLabel("c-inv"), HashFromLabel("c-inv-p"), 7 * COIN, 0, false);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w, catalog, chas);

    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w, catalog, chas, candidate, std::nullopt);

    BOOST_CHECK(!result.condition.has_invariant_pass);
    BOOST_CHECK(HasLayer(result.missing_layers, "FVR-E-invariants"));
    BOOST_CHECK_EQUAL(result.status, Consensus::EquivalenceClosureStatus::Prohibited);
}

BOOST_FIXTURE_TEST_CASE(closure_missing_chas_alignment, ClosureFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(8, HashFromLabel("c-ch"), HashFromLabel("c-ch-p"), 8 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);

    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w, catalog, std::nullopt, std::nullopt, std::nullopt);

    BOOST_CHECK(!result.condition.has_chas_alignment_layer);
    BOOST_CHECK(HasLayer(result.missing_layers, "FVR-H-chas-alignment"));
}

BOOST_FIXTURE_TEST_CASE(closure_missing_global_candidate_layer, ClosureFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(9, HashFromLabel("c-gl"), HashFromLabel("c-gl-p"), 9 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w, catalog, chas);

    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w, catalog, chas, candidate, std::nullopt);

    BOOST_CHECK(!result.condition.has_global_candidate_equivalence_layer);
    BOOST_CHECK(HasLayer(result.missing_layers, "FVR-K-global-candidate-equivalence"));
}

BOOST_FIXTURE_TEST_CASE(closure_full_system_partial_when_draw_blocks, ClosureFixture)
{
    const uint256 h0 = HashFromLabel("c-f0");
    const uint256 h1 = HashFromLabel("c-f1");
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

    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w1, catalog, chas, candidate, global);

    BOOST_CHECK(result.condition.has_runtime_witness_chain);
    BOOST_CHECK(result.condition.has_invariant_pass);
    BOOST_CHECK(result.condition.has_candidate_equivalence_layer);
    BOOST_CHECK(result.condition.has_global_candidate_equivalence_layer);
    BOOST_CHECK(result.condition.has_chas_alignment_layer);
    BOOST_CHECK(result.condition.has_coq_catalog_alignment_layer);
    BOOST_CHECK(!result.condition.no_fundamentally_blocked_draw);
    BOOST_CHECK_EQUAL(result.status, Consensus::EquivalenceClosureStatus::Prohibited);
    BOOST_CHECK(!result.closure_allowed);
    BOOST_CHECK(HasLayer(result.missing_layers, "DRAW-proof-gate"));
}

BOOST_FIXTURE_TEST_CASE(closure_ready_when_all_conditions_met, ClosureFixture)
{
    Consensus::EquivalenceClosureCondition condition;
    condition.has_runtime_witness_chain = true;
    condition.has_invariant_pass = true;
    condition.has_candidate_equivalence_layer = true;
    condition.has_global_candidate_equivalence_layer = true;
    condition.has_chas_alignment_layer = true;
    condition.has_coq_catalog_alignment_layer = true;
    condition.no_fundamentally_blocked_draw = true;

    BOOST_CHECK(Consensus::ClosureAllowed(condition));

    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(1, HashFromLabel("c-rd"), HashFromLabel("c-rd-p"), 1 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);

    Consensus::WitnessEquivalenceCandidate candidate;
    candidate.height = w.height;
    candidate.block_hash = w.block_hash;
    candidate.invariant_status = Consensus::CandidateEquivalenceStatus::Comparable;
    candidate.witness_chain_status = Consensus::CandidateEquivalenceStatus::Comparable;
    candidate.oru_supply_status = Consensus::CandidateEquivalenceStatus::Comparable;
    candidate.res_supply_status = Consensus::CandidateEquivalenceStatus::Comparable;

    const Consensus::GlobalCandidateEquivalenceObservation global =
        Consensus::BuildGlobalCandidateEquivalence(chas, candidate);

    Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w, catalog, chas, candidate, global);
    result.condition = condition;
    result.status = Consensus::EquivalenceClosureStatus::Closable;
    result.closure_allowed = Consensus::ClosureAllowed(condition);

    BOOST_CHECK_EQUAL(result.status, Consensus::EquivalenceClosureStatus::Closable);
    BOOST_CHECK(result.closure_allowed);

    const UniValue json = Consensus::EquivalenceClosureResultToJson(result);
    BOOST_CHECK_EQUAL(json["closure_status"].get_str(), "READY");
    BOOST_CHECK_EQUAL(json["closure_allowed"].get_bool(), true);
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
}

BOOST_FIXTURE_TEST_CASE(closure_draw_prohibited, ClosureFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(2, HashFromLabel("c-dr"), HashFromLabel("c-dr-p"), 2 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::WitnessEquivalenceCandidate candidate =
        Consensus::BuildWitnessEquivalenceCandidate(w, catalog, chas);

    const Consensus::EquivalenceClosureResult result = Consensus::BuildEquivalenceClosure(
        w, catalog, chas, candidate, std::nullopt);

    BOOST_CHECK_EQUAL(result.status, Consensus::EquivalenceClosureStatus::Prohibited);
    BOOST_CHECK(!result.closure_allowed);
}

BOOST_FIXTURE_TEST_CASE(closure_resolve_missing_witness_nullopt, ClosureFixture)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 77;
    BOOST_CHECK(!Consensus::ResolveEquivalenceClosureAt(&pindex).has_value());
}

BOOST_AUTO_TEST_SUITE_END()
