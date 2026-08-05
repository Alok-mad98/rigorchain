// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_coq_chas_adapter.h>
#include <consensus/runtime_supply_witness_chain.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/rigor_consensus_lock.h>
#include <core_io.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <algorithm>
#include <cstring>

BOOST_AUTO_TEST_SUITE(runtime_supply_coq_chas_adapter_tests)

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
    m.res_reward_amount = Consensus::RES_REWARD_AMOUNT;
    return m;
}

struct ChasFixture : BasicTestingSetup {
    ChasFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
    ~ChasFixture() { Consensus::ClearRuntimeSupplyWitnessCacheForTest(); }
};

} // namespace

BOOST_FIXTURE_TEST_CASE(build_chas_vectors_from_catalog_row, ChasFixture)
{
    const uint256 h1 = HashFromLabel("chas-block-1");
    const uint256 h0 = HashFromLabel("chas-block-0");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(0, h0, uint256{}, 50 * COIN, 0));
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(1, h1, h0, 51 * COIN, 0));

    const std::optional<Consensus::RuntimeSupplyWitness> w1 = Consensus::GetRuntimeSupplyWitness(1);
    const std::optional<Consensus::RuntimeSupplyWitness> w0 = Consensus::GetRuntimeSupplyWitness(0);
    BOOST_REQUIRE(w1.has_value());
    BOOST_REQUIRE(w0.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w1, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, w0);
    const Consensus::RuntimeSupplyChasAlignmentReport report =
        Consensus::BuildChasAlignmentFromCatalogRow(row);

    BOOST_CHECK_EQUAL(report.chas_oru_state.chas_field, "chas_oru_state");
    BOOST_CHECK_EQUAL(report.chas_oru_state.coq_catalog_field, "rs_oru_supply");
    BOOST_CHECK_EQUAL(report.chas_res_state.coq_catalog_field, "rs_res_supply");
    BOOST_CHECK_EQUAL(report.chas_oru_state.status, Consensus::CanonicalHashAlignmentStatus::Partial);
    BOOST_CHECK_EQUAL(report.chas_res_state.status, Consensus::CanonicalHashAlignmentStatus::Partial);
    BOOST_CHECK(!report.equivalence_claimed);
}

BOOST_FIXTURE_TEST_CASE(delta_matches_hook_partial_with_mirror, ChasFixture)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    const uint256 parent_hash = HashFromLabel("chas-parent");
    const uint256 child_hash = HashFromLabel("chas-child");
    const CAmount subsidy = GetBlockSubsidy(10, consensus);
    const CAmount burn = COIN;
    const CAmount parent_oru = 100 * COIN;
    const CAmount child_oru = parent_oru + subsidy - burn;

    const Consensus::RuntimeSupplyWitness parent =
        MakeWitness(9, parent_hash, HashFromLabel("p8"), parent_oru, 0);
    const Consensus::RuntimeSupplyWitness child =
        MakeWitness(10, child_hash, parent_hash, child_oru, Consensus::RES_REWARD_AMOUNT);
    Consensus::StoreRuntimeSupplyWitness(parent);
    Consensus::StoreRuntimeSupplyWitness(child);

    const Consensus::RuntimeSupplyState mirror = MakeMirror(
        10, child_hash, parent_hash, parent_oru, child_oru, subsidy, burn, 0,
        Consensus::RES_REWARD_AMOUNT, 1);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        child, mirror, true, Consensus::RuntimeSupplyMirrorSource::STORED_MIRROR, parent);
    const Consensus::RuntimeSupplyChasAlignmentReport report =
        Consensus::BuildChasAlignmentFromCatalogRow(row);

    BOOST_CHECK_EQUAL(report.delta.rs_oru_supply_delta, subsidy - burn);
    BOOST_CHECK_EQUAL(report.delta.rs_res_supply_delta, Consensus::RES_REWARD_AMOUNT);
    BOOST_CHECK(report.delta_audit_violations.empty());
    BOOST_CHECK_EQUAL(report.chas_oru_state.status, Consensus::CanonicalHashAlignmentStatus::Partial);
    BOOST_CHECK(report.chas_oru_state.audit_notes.end() !=
                std::find(report.chas_oru_state.audit_notes.begin(),
                          report.chas_oru_state.audit_notes.end(),
                          "COQ-BURN-DELTA-GAP"));
}

BOOST_FIXTURE_TEST_CASE(genesis_missing_parent_partial, ChasFixture)
{
    const uint256 h0 = HashFromLabel("chas-genesis");
    Consensus::StoreRuntimeSupplyWitness(MakeWitness(0, h0, uint256{}, 50 * COIN, 0));
    const std::optional<Consensus::RuntimeSupplyWitness> w0 = Consensus::GetRuntimeSupplyWitness(0);
    BOOST_REQUIRE(w0.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w0, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport report =
        Consensus::BuildChasAlignmentFromCatalogRow(row);

    BOOST_CHECK(!report.delta.parent_witness_present);
    BOOST_CHECK(report.delta.witness_chain_step_valid);
    BOOST_CHECK_EQUAL(report.chas_oru_state.status, Consensus::CanonicalHashAlignmentStatus::Partial);
}

BOOST_FIXTURE_TEST_CASE(broken_chain_fundamentally_blocked, ChasFixture)
{
    const uint256 h4 = HashFromLabel("chas-h4");
    const Consensus::RuntimeSupplyWitness parent =
        MakeWitness(4, h4, HashFromLabel("chas-h3"), 10 * COIN, 0);
    const Consensus::RuntimeSupplyWitness orphan =
        MakeWitness(5, HashFromLabel("chas-bad"), HashFromLabel("wrong"), 99 * COIN, 0);

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        orphan, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, parent);
    const Consensus::RuntimeSupplyChasAlignmentReport report =
        Consensus::BuildChasAlignmentFromCatalogRow(row);

    BOOST_CHECK_EQUAL(report.chas_oru_state.status,
                      Consensus::CanonicalHashAlignmentStatus::FundamentallyBlocked);
    BOOST_CHECK(!report.delta_audit_violations.empty());
}

BOOST_FIXTURE_TEST_CASE(invariant_failure_fundamentally_blocked, ChasFixture)
{
    Consensus::RuntimeSupplyWitness bad =
        MakeWitness(2, HashFromLabel("bad-inv"), HashFromLabel("p1"), -1, 0, false);
    bad.violations = {"ORU-L1"};
    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        bad, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, std::nullopt);
    const Consensus::RuntimeSupplyChasAlignmentReport report =
        Consensus::BuildChasAlignmentFromCatalogRow(row);

    BOOST_CHECK_EQUAL(report.chas_oru_state.status,
                      Consensus::CanonicalHashAlignmentStatus::FundamentallyBlocked);
    BOOST_CHECK(!report.invariants_passed);
}

BOOST_FIXTURE_TEST_CASE(rpc_alignment_json_shape, ChasFixture)
{
    Consensus::StoreRuntimeSupplyWitness(
        MakeWitness(100, HashFromLabel("chas-rpc"), HashFromLabel("chas-rpc-p"), 123456, 7800));
    const std::optional<Consensus::RuntimeSupplyWitness> w = Consensus::GetRuntimeSupplyWitness(100);
    BOOST_REQUIRE(w.has_value());

    const Consensus::RuntimeSupplyWitnessCoqCatalogRow row = Consensus::BuildWitnessCoqCatalogRow(
        *w, std::nullopt, false, Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH, std::nullopt);
    const UniValue json = Consensus::ChasAlignmentReportToJson(
        Consensus::BuildChasAlignmentFromCatalogRow(row));

    BOOST_CHECK_EQUAL(json["equivalence_claimed"].get_bool(), false);
    BOOST_CHECK_EQUAL(json["coq_equiv_runtime"].get_str(), "NOT_ESTABLISHED");
    BOOST_CHECK(json["chas_field_alignments"].isArray());
    BOOST_CHECK_EQUAL(json["chas_field_alignments"].size(), 2U);
    BOOST_CHECK(json["chas_summary"].isObject());
    BOOST_CHECK_EQUAL(json["chas_summary"]["chas_oru_status"].get_str(), "CHAS_Partial");
    BOOST_CHECK_EQUAL(json["chas_summary"]["pd_4f_ob_02"].get_str(), "OPEN");
}

BOOST_FIXTURE_TEST_CASE(resolve_missing_witness_returns_nullopt, ChasFixture)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 99;
    BOOST_CHECK(!Consensus::ResolveChasAlignmentAt(&pindex).has_value());
}

BOOST_AUTO_TEST_SUITE_END()
