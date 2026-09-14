// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/coq_supply_field_hook.h>
#include <node/supply_monitor/observational_metrics.h>
#include <node/supply_monitor/runtime_supply_export.h>
#include <node/supply_monitor/runtime_supply_state.h>

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <validation.h>

#include <cstring>
#include <optional>

BOOST_FIXTURE_TEST_SUITE(runtime_supply_state_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(runtime_supply_mirror_matches_authoritative_index_fields)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(), "RUNTIME-SUPPLY-MIRROR-TEST-TIP!", 32);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 42;
    pindex.phashBlock = &fake_tip;
    pindex.m_block_converge_sum = 3 * COIN;
    // Use a synthetic RES mint value for testing (RES schedule removed in RFC-019)
    const CAmount block_res_mint = 1 * COIN;
    pindex.m_block_res_mint_sum = block_res_mint;
    pindex.Observational().LoadCachesForTest(block_res_mint, 1000 * COIN);

    const CAmount parent_oru = 1000 * COIN - GetBlockSubsidy(pindex.nHeight, consensus) + pindex.m_block_converge_sum;
    const CAmount parent_res = 0;

    const Consensus::RuntimeSupplyState mirror = Consensus::CaptureRuntimeSupplyState(
        &pindex,
        view,
        consensus,
        /*res_mint_count_block=*/1,
        std::optional<CAmount>{parent_oru},
        std::optional<CAmount>{parent_res});

    BOOST_CHECK_EQUAL(mirror.height, pindex.nHeight);
    BOOST_CHECK_EQUAL(mirror.block_hash, pindex.GetBlockHash());
    BOOST_CHECK_EQUAL(mirror.oru_supply_cumulative, pindex.Observational().GetChainOruTotal());
    BOOST_CHECK_EQUAL(mirror.oru_converge_burn_block, pindex.m_block_converge_sum);
    BOOST_CHECK_EQUAL(mirror.oru_subsidy_block, GetBlockSubsidy(pindex.nHeight, consensus));
    BOOST_CHECK_EQUAL(mirror.res_mint_cumulative, pindex.Observational().GetChainResMintTotal());
    BOOST_CHECK_EQUAL(mirror.res_mint_block, pindex.m_block_res_mint_sum);
    BOOST_CHECK_EQUAL(mirror.res_mint_present_block, 1);
    BOOST_CHECK_EQUAL(mirror.oru_supply_parent, parent_oru);
    BOOST_CHECK_EQUAL(mirror.res_mint_parent, parent_res);

    const std::vector<std::string> violations = Consensus::CheckRuntimeSupplyInvariants(mirror, consensus);
    if (!violations.empty()) {
        BOOST_ERROR(violations.front());
    }
    BOOST_CHECK(violations.empty());
}

BOOST_AUTO_TEST_CASE(runtime_supply_recurrence_and_cap_invariants)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    Consensus::RuntimeSupplyState s;
    s.height = 1;
    s.oru_supply_parent = 50 * COIN;
    s.oru_subsidy_block = GetBlockSubsidy(1, consensus);
    s.oru_converge_burn_block = COIN;
    s.oru_supply_cumulative = s.oru_supply_parent + s.oru_subsidy_block - s.oru_converge_burn_block;
    s.res_mint_parent = 0;
    const CAmount res_mint = 1 * COIN;  // Synthetic RES mint for testing
    s.res_mint_block = res_mint;
    s.res_mint_present_block = 1;
    s.res_mint_cumulative = s.res_mint_parent + s.res_mint_block;
    s.res_max_supply = Consensus::RES_MAX_SUPPLY;
    s.utxo_res_balance = res_mint;
    s.utxo_non_res_balance = s.oru_supply_cumulative;

    BOOST_CHECK(Consensus::CheckRuntimeSupplyInvariants(s, consensus).empty());
}

BOOST_AUTO_TEST_CASE(runtime_supply_mirror_store_and_invalidate)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(), "RUNTIME-SUPPLY-STORE-INVALIDATE!", 32);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 0;
    pindex.phashBlock = &fake_tip;
    pindex.m_block_converge_sum = 0;
    pindex.m_block_res_mint_sum = 0;
    pindex.Observational().LoadCachesForTest(0, GetBlockSubsidy(0, consensus));

    Consensus::RuntimeSupplyState mirror = Consensus::CaptureRuntimeSupplyState(
        &pindex, view, consensus, 0, CAmount{0}, CAmount{0});
    Consensus::StoreRuntimeSupplyMirror(&pindex, std::move(mirror));
    BOOST_REQUIRE(pindex.m_runtime_supply_mirror.has_value());

    Consensus::InvalidateRuntimeSupplyMirror(&pindex);
    BOOST_CHECK(!pindex.m_runtime_supply_mirror.has_value());
}

BOOST_AUTO_TEST_CASE(coq_field_hook_matches_mirror_and_authoritative_caches)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(), "COQ-FIELD-HOOK-MATCH-CACHE-TEST!", 32);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 10;
    pindex.phashBlock = &fake_tip;
    pindex.m_block_converge_sum = 2 * COIN;
    pindex.m_block_res_mint_sum = 0;
    pindex.Observational().LoadCachesForTest(0, 50 * COIN);

    const CAmount parent_oru =
        pindex.Observational().GetChainOruTotal() - GetBlockSubsidy(pindex.nHeight, consensus) + pindex.m_block_converge_sum;

    const Consensus::RuntimeSupplyState mirror = Consensus::CaptureRuntimeSupplyState(
        &pindex, view, consensus, 0, std::optional<CAmount>{parent_oru}, std::optional<CAmount>{0});

    const Consensus::CoqSupplyFieldHook hook = Consensus::ProjectCoqSupplyFieldHook(mirror);

    BOOST_CHECK_EQUAL(hook.rs_oru_supply, mirror.oru_supply_cumulative);
    BOOST_CHECK_EQUAL(hook.rs_res_supply, mirror.res_mint_cumulative);
    BOOST_CHECK_EQUAL(hook.cb_oru_subsidy, mirror.oru_subsidy_block);
    BOOST_CHECK_EQUAL(hook.cb_oru_burn_delta, mirror.oru_converge_burn_block);
    BOOST_CHECK_EQUAL(hook.cb_res_mint_delta, mirror.res_mint_block);
    BOOST_CHECK_EQUAL(hook.res_mint_count_M_h, mirror.res_mint_present_block);
    BOOST_CHECK_EQUAL(hook.audit_coq_cb_oru_mint_delta_scaffold, hook.cb_oru_subsidy);

    const std::vector<std::string> coq_errors =
        Consensus::CheckCoqHookMatchesAuthoritativeCaches(hook, mirror, pindex);
    if (!coq_errors.empty()) {
        BOOST_ERROR(coq_errors.front());
    }
    BOOST_CHECK(coq_errors.empty());

    const Consensus::CoqSupplyFieldHook resolved =
        Consensus::ResolveCoqSupplyFieldHook(&pindex, view, consensus);
    BOOST_CHECK_EQUAL(resolved.rs_oru_supply, hook.rs_oru_supply);
    BOOST_CHECK_EQUAL(resolved.cb_oru_burn_delta, pindex.m_block_converge_sum);
}

BOOST_AUTO_TEST_CASE(coq_hook_recurrence_matches_oru_res_ledgers)
{
    Consensus::CoqSupplyFieldHook hook;
    hook.height = 3;
    hook.rs_oru_supply = 100;
    hook.rs_res_supply = 200;
    hook.cb_oru_subsidy = 50;
    hook.cb_oru_burn_delta = 10;
    hook.cb_res_mint_delta = 100;
    hook.res_mint_count_M_h = 1;
    hook.rs_oru_supply_parent = 60;
    hook.rs_res_supply_parent = 100;
    hook.audit_coq_cb_oru_mint_delta_scaffold = 50;
    BOOST_CHECK_EQUAL(hook.rs_oru_supply, hook.rs_oru_supply_parent + hook.cb_oru_subsidy - hook.cb_oru_burn_delta);
    BOOST_CHECK_EQUAL(hook.rs_res_supply, hook.rs_res_supply_parent + hook.cb_res_mint_delta);
    BOOST_CHECK(hook.res_mint_count_M_h <= 1);
}

BOOST_AUTO_TEST_CASE(runtime_supply_export_phase5_cell_count_and_mirror_source)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(), "RUNTIME-SUPPLY-EXPORT-PHASE5-TEST!", 32);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 7;
    pindex.phashBlock = &fake_tip;
    pindex.m_block_converge_sum = COIN;
    const CAmount res_mint = 1 * COIN;  // Synthetic RES mint for testing
    pindex.m_block_res_mint_sum = res_mint;
    pindex.Observational().LoadCachesForTest(res_mint, 20 * COIN);

    const CAmount parent_oru =
        pindex.Observational().GetChainOruTotal() - GetBlockSubsidy(pindex.nHeight, consensus) + pindex.m_block_converge_sum;

    Consensus::RuntimeSupplyState mirror = Consensus::CaptureRuntimeSupplyState(
        &pindex, view, consensus, 1, std::optional<CAmount>{parent_oru}, std::optional<CAmount>{0});
    Consensus::StoreRuntimeSupplyMirror(&pindex, std::move(mirror));

    const Consensus::RuntimeSupplyExport exp =
        Consensus::BuildRuntimeSupplyExport(&pindex, view, consensus);

    BOOST_CHECK(exp.mirror_source == Consensus::RuntimeSupplyMirrorSource::STORED_MIRROR);
    BOOST_CHECK_EQUAL(exp.mirror.oru_supply_cumulative, pindex.Observational().GetChainOruTotal());
    BOOST_CHECK_EQUAL(exp.authoritative_oru_converge_burn_block, pindex.m_block_converge_sum);
    BOOST_CHECK_EQUAL(exp.coq_hook.rs_oru_supply, exp.mirror.oru_supply_cumulative);

    const UniValue cells = Consensus::RuntimeSupplyPhase5CellsToJson(exp);
    BOOST_CHECK(cells.isArray());
    // 15 observables x 2 sources. Was 32 before RFC-019 retired the res_reward_amount
    // pair (runtime_mirror + authoritative_index), which had mirrored the W(h) schedule.
    BOOST_CHECK_EQUAL(cells.size(), 30U);

    const UniValue json = Consensus::RuntimeSupplyExportToJson(exp, pindex, consensus);
    BOOST_CHECK(json["mirror_present"].get_bool());
    BOOST_CHECK_EQUAL(json["mirror_source"].get_str(), "stored_mirror");
    // The reported count must be the array it describes, not an independent literal.
    BOOST_CHECK_EQUAL(json["phase5_supply_cell_count"].getInt<int>(), static_cast<int>(cells.size()));
    BOOST_CHECK(json["coq_hook_violations"].empty());
}

BOOST_AUTO_TEST_CASE(runtime_supply_export_fresh_capture_when_mirror_absent)
{
    const Consensus::Params& consensus = Params().GetConsensus();

    CCoinsView base;
    CCoinsViewCache view(&base);

    uint256 fake_tip;
    std::memcpy(fake_tip.begin(), "RUNTIME-SUPPLY-EXPORT-FRESH-CAP!", 32);

    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    pindex.nHeight = 2;
    pindex.phashBlock = &fake_tip;
    pindex.m_block_converge_sum = 0;
    pindex.m_block_res_mint_sum = 0;
    pindex.Observational().LoadCachesForTest(0, GetBlockSubsidy(0, consensus) + GetBlockSubsidy(1, consensus));

    const Consensus::RuntimeSupplyExport exp =
        Consensus::BuildRuntimeSupplyExport(&pindex, view, consensus);

    BOOST_CHECK(exp.mirror_source == Consensus::RuntimeSupplyMirrorSource::CAPTURED_FRESH);
    BOOST_CHECK(!pindex.m_runtime_supply_mirror.has_value());
    BOOST_CHECK_EQUAL(exp.mirror.oru_supply_cumulative, pindex.Observational().GetChainOruTotal());
}

BOOST_AUTO_TEST_CASE(derived_oru_subsidy_cumulative_matches_manual_sum)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    const int heights[] = {0, 1, 5, 149, 150, 151, 449, 450, 1000};
    for (int h : heights) {
        CAmount manual = 0;
        for (int i = 0; i <= h; ++i) {
            manual += GetBlockSubsidy(i, consensus);
        }
        BOOST_CHECK_EQUAL(Consensus::ComputeDerivedOruSubsidyCumulative(h, consensus), manual);
    }
}

BOOST_AUTO_TEST_CASE(observational_metrics_update_after_connect_shape)
{
    CBlockHeader hdr{};
    CBlockIndex pindex{hdr};
    CBlock block;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(cb)));

    pindex.Observational().Update(block, /*pindexPrev=*/nullptr, 5 * COIN, 100 * COIN);
    BOOST_CHECK_EQUAL(pindex.Observational().GetChainResMintTotal(), 5 * COIN);
    BOOST_CHECK_EQUAL(pindex.Observational().GetChainOruTotal(), 100 * COIN);
    BOOST_CHECK_EQUAL(pindex.Observational().GetCumulativeConvergence(), 0U);

    pindex.Observational().InvalidateLedgerCaches();
    BOOST_CHECK(!pindex.Observational().HasResMintCache());
    BOOST_CHECK(!pindex.Observational().HasOruCache());
    BOOST_CHECK_EQUAL(pindex.Observational().GetCumulativeConvergence(), 0U);
}

BOOST_AUTO_TEST_CASE(res_mint_fold_invalid_is_not_rewritten_to_zero)
{
    CBlockHeader parent_hdr{};
    CBlockIndex parent{parent_hdr};
    parent.nHeight = 1;
    parent.m_block_res_mint_sum = 0;
    parent.Observational().WriteResMintCache(Consensus::RES_MAX_SUPPLY + 1);

    CBlockHeader child_hdr{};
    CBlockIndex child{child_hdr};
    child.nHeight = 2;
    child.pprev = &parent;
    child.m_block_res_mint_sum = COIN;

    const ObservationalFoldResult fold = FoldChainResMintTotal(&child);
    BOOST_CHECK(!fold.ok());
    BOOST_CHECK(!child.Observational().HasResMintCache());

    CBlock block;
    CMutableTransaction cb;
    cb.vin.resize(1);
    cb.vin[0].prevout.SetNull();
    block.vtx.push_back(MakeTransactionRef(std::move(cb)));

    // ConnectBlock must leave the ledger unset, not cache 0 + this-block mint.
    child.Observational().Update(block, &parent, std::nullopt, std::nullopt);
    BOOST_CHECK(!child.Observational().HasResMintCache());
    BOOST_CHECK(!child.Observational().HasOruCache());
    BOOST_CHECK_EQUAL(child.Observational().GetChainResMintTotal(), -1);
}

BOOST_AUTO_TEST_CASE(res_mint_fold_ok_is_not_confused_with_invalid)
{
    CBlockHeader parent_hdr{};
    CBlockIndex parent{parent_hdr};
    parent.nHeight = 1;
    parent.m_block_res_mint_sum = 2 * COIN;
    parent.Observational().WriteResMintCache(2 * COIN);

    CBlockHeader child_hdr{};
    CBlockIndex child{child_hdr};
    child.nHeight = 2;
    child.pprev = &parent;
    child.m_block_res_mint_sum = 3 * COIN;

    const ObservationalFoldResult fold = FoldChainResMintTotal(&child);
    BOOST_REQUIRE(fold.ok());
    BOOST_CHECK_EQUAL(fold.value, 5 * COIN);
    BOOST_CHECK(child.Observational().HasResMintCache());
    BOOST_CHECK_EQUAL(child.Observational().GetChainResMintTotal(), 5 * COIN);
}

BOOST_AUTO_TEST_SUITE_END()
