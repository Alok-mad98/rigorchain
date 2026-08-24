// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proof/runtime_vector_harness.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <consensus/runtime_supply_witness_chain.h>
#include <uint256.h>
#include <consensus/coq_supply_field_hook.h>
#include <consensus/rigor_consensus_lock.h>
#include <test/util/setup_common.h>
#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(runtime_vector_harness_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(registry_has_twelve_vectors)
{
    BOOST_CHECK_EQUAL(proof::GOLDEN_VECTOR_COUNT, 12U);
    BOOST_CHECK_EQUAL(proof::GetGoldenVectorSpec(0).id, std::string{"GV-ORU-0001"});
    BOOST_CHECK_EQUAL(proof::GetGoldenVectorSpec(11).id, std::string{"GV-STATE-0003"});
}

BOOST_AUTO_TEST_CASE(all_vectors_not_executed_before_fixtures)
{
    proof::ResetGoldenVectorFixtureFlagsForTest();
    proof::RuntimeWitnessSnapshot empty;
    for (size_t i = 0; i < proof::GOLDEN_VECTOR_COUNT; ++i) {
        const proof::GoldenVectorSpec& spec = proof::GetGoldenVectorSpec(i);
        const proof::VectorVerificationReport report = proof::EvaluateGoldenVector(spec, empty);
        BOOST_CHECK_EQUAL(report.vector_id, spec.id);
        BOOST_CHECK_EQUAL(report.verification_status, proof::GvrVerificationStatus::NOT_EXECUTED);
        BOOST_CHECK(!report.runtime_witness_present);
        BOOST_CHECK_EQUAL(report.mismatch_count, 0);
        BOOST_CHECK(!report.notes.empty());
    }
}

BOOST_AUTO_TEST_CASE(batch_report_defaults_to_not_executed)
{
    proof::ResetGoldenVectorFixtureFlagsForTest();
    const std::vector<proof::VectorVerificationReport> reports =
        proof::EvaluateAllGoldenVectors(nullptr, nullptr, nullptr);
    BOOST_REQUIRE_EQUAL(reports.size(), proof::GOLDEN_VECTOR_COUNT);
    for (const proof::VectorVerificationReport& report : reports) {
        BOOST_CHECK_EQUAL(report.verification_status, proof::GvrVerificationStatus::NOT_EXECUTED);
    }
    const UniValue batch = proof::GoldenVectorBatchReportToJson(reports);
    BOOST_CHECK(batch["equivalence_claimed"].get_bool() == false);
    BOOST_CHECK_EQUAL(batch["summary"]["not_executed"].getInt<int>(), 12);
    BOOST_CHECK_EQUAL(batch["summary"]["pass"].getInt<int>(), 0);
}

BOOST_AUTO_TEST_CASE(gv_oru_0001_pass_with_synthetic_witness)
{
    proof::ResetGoldenVectorFixtureFlagsForTest();
    proof::SetGoldenVectorFixtureMaterializedForTest(0, true);

    const Consensus::Params& consensus = Params().GetConsensus();

    proof::RuntimeWitnessSnapshot snapshot;
    snapshot.present = true;
    snapshot.export_data.mirror.height = 0;
    snapshot.export_data.mirror.oru_supply_cumulative = 100'000'000'000;
    snapshot.export_data.mirror.oru_supply_parent = 0;
    snapshot.export_data.mirror.oru_subsidy_block = 100'000'000'000;
    snapshot.export_data.mirror.oru_converge_burn_block = 0;
    snapshot.export_data.mirror.res_mint_cumulative = 0;
    snapshot.export_data.mirror.res_mint_block = 0;
    snapshot.export_data.mirror.res_mint_count_block = 0;
    snapshot.export_data.mirror.res_max_supply = Consensus::RES_MAX_SUPPLY;
    snapshot.export_data.mirror.res_reward_amount = Consensus::RES_REWARD_GENESIS;

    snapshot.export_data.coq_hook = Consensus::ProjectCoqSupplyFieldHook(snapshot.export_data.mirror);
    snapshot.invariants = Consensus::VerifyRuntimeSupplyInvariants(snapshot.export_data.mirror, consensus);

    const proof::VectorVerificationReport report =
        proof::EvaluateGoldenVector(proof::GetGoldenVectorSpec(0), snapshot);

    BOOST_CHECK_EQUAL(report.verification_status, proof::GvrVerificationStatus::PASS);
    BOOST_CHECK_EQUAL(report.mismatch_count, 0);
    BOOST_CHECK(report.runtime_witness_present);

    const UniValue json = proof::VectorVerificationReportToJson(report);
    BOOST_CHECK_EQUAL(json["verification_status"].get_str(), "GVR_PASS");
    BOOST_CHECK_EQUAL(json["vector_verification"].get_str(), "VERIFIED");
    BOOST_CHECK(json["equivalence_claimed"].get_bool() == false);
}

BOOST_AUTO_TEST_CASE(find_vector_by_id)
{
    const proof::GoldenVectorSpec* spec = proof::FindGoldenVectorSpec("GV-DRAW-0002");
    BOOST_REQUIRE(spec != nullptr);
    BOOST_CHECK_EQUAL(spec->height, 7);
    BOOST_CHECK(spec->draw_fields_optional);
}

BOOST_AUTO_TEST_CASE(draw_vector_partial_with_materialized_fixture)
{
    proof::ResetGoldenVectorFixtureFlagsForTest();
    proof::SetGoldenVectorFixtureMaterializedForTest(7, true);

    proof::RuntimeWitnessSnapshot snapshot;
    snapshot.present = true;
    snapshot.export_data.mirror.height = 7;
    snapshot.export_data.mirror.res_mint_cumulative = 2 * Consensus::RES_REWARD_GENESIS;
    snapshot.export_data.mirror.res_mint_parent = Consensus::RES_REWARD_GENESIS;
    snapshot.export_data.mirror.res_mint_block = Consensus::RES_REWARD_GENESIS;
    snapshot.export_data.mirror.res_mint_count_block = 1;
    snapshot.export_data.mirror.res_max_supply = Consensus::RES_MAX_SUPPLY;
    snapshot.export_data.mirror.res_reward_amount = Consensus::RES_REWARD_GENESIS;
    snapshot.export_data.coq_hook = Consensus::ProjectCoqSupplyFieldHook(snapshot.export_data.mirror);
    snapshot.invariants = Consensus::VerifyRuntimeSupplyInvariants(
        snapshot.export_data.mirror, Params().GetConsensus());

    const proof::VectorVerificationReport report =
        proof::EvaluateGoldenVector(proof::GetGoldenVectorSpec(7), snapshot);

    BOOST_CHECK_EQUAL(report.verification_status, proof::GvrVerificationStatus::PARTIAL);
    BOOST_CHECK_EQUAL(report.mismatch_count, 0);
}

BOOST_AUTO_TEST_CASE(harness_fails_when_the_observation_disagrees_with_the_vector)
{
    // This case exists because the golden-vector gate spent its whole life unable to fail. The
    // deleted `materializegoldenvector` RPC built the observational mirror out of the vector's own
    // expected values, wrote them into the block index, and then compared the two — so every field
    // matched by construction and "ESTABLISHED" meant nothing. What must be pinned is not that a
    // correct observation passes, which the tautology also did, but that a wrong one FAILS.
    proof::ResetGoldenVectorFixtureFlagsForTest();
    proof::SetGoldenVectorFixtureMaterializedForTest(0, true);

    const Consensus::Params& consensus = Params().GetConsensus();
    const proof::GoldenVectorSpec& spec = proof::GetGoldenVectorSpec(0);

    auto snapshot_with_supply = [&](CAmount oru_supply) {
        proof::RuntimeWitnessSnapshot snapshot;
        snapshot.present = true;
        snapshot.export_data.mirror.height = spec.height;
        snapshot.export_data.mirror.oru_supply_cumulative = oru_supply;
        snapshot.export_data.mirror.oru_supply_parent = spec.expected_oru_parent;
        snapshot.export_data.mirror.oru_subsidy_block = spec.expected_oru_subsidy;
        snapshot.export_data.mirror.oru_converge_burn_block = spec.expected_oru_burn;
        snapshot.export_data.mirror.res_mint_cumulative = spec.expected_res_supply;
        snapshot.export_data.mirror.res_mint_parent = spec.expected_res_parent;
        snapshot.export_data.mirror.res_mint_block = spec.expected_res_mint;
        snapshot.export_data.mirror.res_mint_count_block = spec.expected_res_mint_count;
        snapshot.export_data.mirror.res_max_supply = Consensus::RES_MAX_SUPPLY;
        snapshot.export_data.mirror.res_reward_amount = Consensus::RES_REWARD_GENESIS;
        snapshot.export_data.coq_hook =
            Consensus::ProjectCoqSupplyFieldHook(snapshot.export_data.mirror);
        snapshot.invariants =
            Consensus::VerifyRuntimeSupplyInvariants(snapshot.export_data.mirror, consensus);
        snapshot.witness =
            Consensus::BuildRuntimeSupplyWitness(snapshot.export_data.mirror, *snapshot.invariants);
        return snapshot;
    };

    const proof::VectorVerificationReport agreeing =
        proof::EvaluateGoldenVector(spec, snapshot_with_supply(spec.expected_oru_supply));
    BOOST_CHECK_EQUAL(agreeing.verification_status, proof::GvrVerificationStatus::PASS);
    BOOST_CHECK_EQUAL(agreeing.mismatch_count, 0);

    // One satoshi of disagreement. An invariant break rides along, but that row is diagnostic
    // only, so a FAIL here can only come from the field comparison itself.
    const proof::VectorVerificationReport disagreeing =
        proof::EvaluateGoldenVector(spec, snapshot_with_supply(spec.expected_oru_supply + 1));
    BOOST_CHECK_EQUAL(disagreeing.verification_status, proof::GvrVerificationStatus::FAIL);
    BOOST_CHECK(disagreeing.mismatch_count > 0);
}

BOOST_AUTO_TEST_CASE(invariants_diagnostic_mismatch_does_not_fail_harness)
{
    proof::ResetGoldenVectorFixtureFlagsForTest();
    proof::SetGoldenVectorFixtureMaterializedForTest(0, true);

    const Consensus::Params& consensus = Params().GetConsensus();

    proof::RuntimeWitnessSnapshot snapshot;
    snapshot.present = true;
    snapshot.export_data.mirror.height = 0;
    snapshot.export_data.mirror.oru_supply_cumulative = 100'000'000'000;
    snapshot.export_data.mirror.oru_supply_parent = 0;
    snapshot.export_data.mirror.oru_subsidy_block = 100'000'000'000;
    snapshot.export_data.mirror.oru_converge_burn_block = 0;
    snapshot.export_data.mirror.res_mint_cumulative = 0;
    snapshot.export_data.mirror.res_mint_block = 0;
    snapshot.export_data.mirror.res_mint_count_block = 0;
    snapshot.export_data.mirror.res_max_supply = Consensus::RES_MAX_SUPPLY;
    snapshot.export_data.mirror.res_reward_amount = Consensus::RES_REWARD_GENESIS;
    snapshot.export_data.coq_hook = Consensus::ProjectCoqSupplyFieldHook(snapshot.export_data.mirror);

    Consensus::RuntimeInvariantResult inv =
        Consensus::VerifyRuntimeSupplyInvariants(snapshot.export_data.mirror, consensus);
    inv.passed = false;
    snapshot.invariants = inv;

    const proof::VectorVerificationReport report =
        proof::EvaluateGoldenVector(proof::GetGoldenVectorSpec(0), snapshot);

    BOOST_CHECK_EQUAL(report.verification_status, proof::GvrVerificationStatus::PASS);
    BOOST_CHECK_EQUAL(report.mismatch_count, 0);
    bool saw_invariants_row = false;
    for (const proof::VectorFieldResult& row : report.field_results) {
        if (row.field == "invariants_passed") {
            saw_invariants_row = true;
            BOOST_CHECK(!row.required);
            BOOST_CHECK_EQUAL(row.result, proof::FieldCompareResult::MISMATCH);
        }
    }
    BOOST_CHECK(saw_invariants_row);
}

BOOST_AUTO_TEST_SUITE_END()
