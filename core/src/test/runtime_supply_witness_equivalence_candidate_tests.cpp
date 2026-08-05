// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_coq_chas_adapter.h>
#include <consensus/runtime_supply_witness_equivalence_candidate.h>
#include <consensus/runtime_supply_witness_chain.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <algorithm>
#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_witness_equivalence_candidate_tests)

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

Consensus::WitnessEquivalenceCandidate BuildFromWitnessAndCatalog(
    const Consensus::RuntimeSupplyWitness& witness,
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow& catalog)
{
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    return Consensus::BuildWitnessEquivalenceCandidate(witness, catalog, chas);
}

bool HasBlockingReason(const Consensus::WitnessEquivalenceCandidate& c, const std::string& reason)
{
    return std::find(c.blocking_reasons.begin(), c.blocking_reasons.end(), reason) !=
           c.blocking_reasons.end();
}

struct EquivalenceFixture : BasicTestingSetup {
    EquivalenceFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~EquivalenceFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(comparable_with_parent_delta, EquivalenceFixture)
{
    const uint256 h0 = HashFromLabel("eq-h0");
    const uint256 h1 = HashFromLabel("eq-h1");
    const Consensus::RuntimeSupplyWitness w0 = MakeWitness(0, h0, uint256{}, 50 * COIN, 0);
    const Consensus::RuntimeSupplyWitness w1 = MakeWitness(1, h1, h0, 51 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w0);
    Consensus::StoreRuntimeSupplyWitness(w1);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w1, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, w0);
    const Consensus::WitnessEquivalenceCandidate candidate = BuildFromWitnessAndCatalog(w1, catalog);

    BOOST_CHECK_EQUAL(candidate.oru_supply_status, Consensus::CandidateEquivalenceStatus::Comparable);
    BOOST_CHECK_EQUAL(candidate.res_supply_status, Consensus::CandidateEquivalenceStatus::Comparable);
    BOOST_CHECK_EQUAL(candidate.invariant_status, Consensus::CandidateEquivalenceStatus::Comparable);
    BOOST_CHECK_EQUAL(candidate.witness_chain_status, Consensus::CandidateEquivalenceStatus::Comparable);
    BOOST_CHECK(!candidate.equivalence_claimed);
    BOOST_CHECK(HasBlockingReason(candidate, "DRAW-CES-FundamentallyBlocked"));
}

BOOST_FIXTURE_TEST_CASE(partially_comparable_genesis, EquivalenceFixture)
{
    const uint256 h0 = HashFromLabel("eq-genesis");
    const Consensus::RuntimeSupplyWitness w0 = MakeWitness(0, h0, uint256{}, 50 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w0);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w0, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::WitnessEquivalenceCandidate candidate = BuildFromWitnessAndCatalog(w0, catalog);

    BOOST_CHECK_EQUAL(candidate.oru_supply_status,
                      Consensus::CandidateEquivalenceStatus::PartiallyComparable);
    BOOST_CHECK_EQUAL(candidate.res_supply_status,
                      Consensus::CandidateEquivalenceStatus::PartiallyComparable);
    BOOST_CHECK(HasBlockingReason(candidate, "ORU-PARENT-DELTA-MISSING"));
}

BOOST_FIXTURE_TEST_CASE(fundamentally_blocked_invariants, EquivalenceFixture)
{
    Consensus::RuntimeSupplyWitness bad =
        MakeWitness(3, HashFromLabel("eq-bad"), HashFromLabel("eq-p2"), -1, 0, false);
    bad.violations = {"ORU-L1"};

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(bad, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::WitnessEquivalenceCandidate candidate = BuildFromWitnessAndCatalog(bad, catalog);

    BOOST_CHECK_EQUAL(candidate.invariant_status,
                      Consensus::CandidateEquivalenceStatus::FundamentallyBlocked);
    BOOST_CHECK(HasBlockingReason(candidate, "INVARIANT-CES-FundamentallyBlocked"));
    BOOST_CHECK(HasBlockingReason(candidate, "ORU-L1"));
}

BOOST_FIXTURE_TEST_CASE(broken_witness_chain_blocked, EquivalenceFixture)
{
    const Consensus::RuntimeSupplyWitness parent =
        MakeWitness(4, HashFromLabel("eq-p4"), HashFromLabel("eq-p3"), 10 * COIN, 0);
    const Consensus::RuntimeSupplyWitness orphan =
        MakeWitness(5, HashFromLabel("eq-bad5"), HashFromLabel("wrong"), 99 * COIN, 0);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(orphan, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             parent);
    const Consensus::WitnessEquivalenceCandidate candidate =
        BuildFromWitnessAndCatalog(orphan, catalog);

    BOOST_CHECK_EQUAL(candidate.witness_chain_status,
                      Consensus::CandidateEquivalenceStatus::FundamentallyBlocked);
    BOOST_CHECK_EQUAL(candidate.invariant_status,
                      Consensus::CandidateEquivalenceStatus::FundamentallyBlocked);
    BOOST_CHECK(HasBlockingReason(candidate, "WITNESS-CHAIN-CES-FundamentallyBlocked"));
}

BOOST_FIXTURE_TEST_CASE(missing_witness_not_comparable, EquivalenceFixture)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 77;
    BOOST_CHECK(!Consensus::ResolveWitnessEquivalenceCandidateAt(&pindex).has_value());
}

BOOST_FIXTURE_TEST_CASE(draw_always_blocked, EquivalenceFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(2, HashFromLabel("eq-d2"), HashFromLabel("eq-d1"), 5 * COIN, 0);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::WitnessEquivalenceCandidate candidate = BuildFromWitnessAndCatalog(w, catalog);

    BOOST_CHECK(HasBlockingReason(candidate, "DRAW-CES-FundamentallyBlocked"));
    BOOST_CHECK(HasBlockingReason(candidate, "PD-4F-OB-02-OPEN"));
    BOOST_CHECK(HasBlockingReason(candidate, "PD-4F-CPP-01-OPEN"));
}

BOOST_FIXTURE_TEST_CASE(rpc_json_shape, EquivalenceFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(10, HashFromLabel("eq-rpc"), HashFromLabel("eq-rpc-p"), 100 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const UniValue json = Consensus::WitnessEquivalenceCandidateToJson(
        BuildFromWitnessAndCatalog(w, catalog));

    BOOST_CHECK_EQUAL(json["height"].getInt<int>(), 10);
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    BOOST_CHECK(json["oru_supply_status"].isStr());
    BOOST_CHECK(json["blocking_reasons"].isArray());
}

BOOST_FIXTURE_TEST_CASE(coq_export_json_shape, EquivalenceFixture)
{
    const Consensus::RuntimeSupplyWitness w =
        MakeWitness(20, HashFromLabel("coq-exp"), HashFromLabel("coq-exp-p"), 200 * COIN, 0);
    Consensus::StoreRuntimeSupplyWitness(w);
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow catalog =
        Consensus::BuildWitnessCoqCatalogRow(w, std::nullopt, false,
                                             Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH,
                                             std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport chas =
        Consensus::BuildChasAlignmentFromCatalogRow(catalog);
    const Consensus::CoqEquivalenceCandidateExport exp{
        BuildFromWitnessAndCatalog(w, catalog), w, catalog, chas};
    const UniValue json = Consensus::CoqEquivalenceCandidateExportToJson(exp);

    BOOST_CHECK_EQUAL(json["task"].get_str(), "FVR-J");
    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK(json["runtime_candidate"].isObject());
    BOOST_CHECK(json["coq_formal_layer"].isObject());
    BOOST_CHECK(json["coq_formal_layer"]["RuntimeWitness"].isObject());
    BOOST_CHECK(json["coq_formal_layer"]["CoqWitnessRow"].isObject());
    BOOST_CHECK(json["coq_formal_layer"]["CHASReport"].isObject());
    BOOST_CHECK(json["coq_formal_layer"]["CandidateEquivalenceObservation"].isObject());
    BOOST_CHECK_EQUAL(json["coq_formal_layer"]["CandidateEquivalenceObservation"]["ceo_oru_supply_status"].get_str(),
                      "CES_PartiallyComparable");
}

BOOST_AUTO_TEST_SUITE_END()
