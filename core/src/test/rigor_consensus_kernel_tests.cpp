// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-047 Consensus Kernel first cut. Production schedules are not redefined.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root_eligibility.h>
#include <consensus/params.h>
#include <consensus/rigor_coinbase_layout.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_math.h>
#include <consensus/rigor_supply.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/rigor_bytes.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <utility>
#include <vector>

namespace {

constexpr CAmount kSaturatingWeight =
    65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

CScript OruP2wpkh()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

CScript ResNative()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(rigor_consensus_kernel_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(issuance_height_epoch_oru_res_closed)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(c.nSubsidyHalvingInterval, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    BOOST_REQUIRE_EQUAL(Consensus::RES_REWARD_EPOCH, Consensus::ORU_SUBSIDY_HALVING_INTERVAL);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210000), 1);

    const int heights[] = {0, 1, 209999, 210000, 210001, 419999, 420000};
    for (int h : heights) {
        const RigorIssuance got = ComputeRigorIssuance(h, c);
        BOOST_CHECK_EQUAL(got.epoch, RigorIssuanceEpoch(h));
        BOOST_CHECK_EQUAL(got.epoch, h / Consensus::RES_REWARD_EPOCH);
        BOOST_CHECK_EQUAL(got.oru_subsidy, GetBlockSubsidy(h, c));
        BOOST_CHECK_EQUAL(got.res_reward, Consensus::GetResMintReward(h));
    }

    BOOST_CHECK_EQUAL(ComputeRigorIssuance(0, c).oru_subsidy, 0);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(0, c).res_reward, Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(1, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(209999, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(210000, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(210001, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(419999, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(420000, c).oru_subsidy, Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);

    const RigorIssuance last0 = ComputeRigorIssuance(209999, c);
    const RigorIssuance first1 = ComputeRigorIssuance(210000, c);
    BOOST_CHECK_EQUAL(last0.epoch, 0);
    BOOST_CHECK_EQUAL(first1.epoch, 1);
    BOOST_CHECK_LT(first1.oru_subsidy, last0.oru_subsidy);
    BOOST_CHECK_LT(first1.res_reward, last0.res_reward);
}

BOOST_AUTO_TEST_CASE(supply_delta_paths_are_not_degenerate)
{
    const auto& c = Params().GetConsensus();
    const int h = 1;
    const CAmount subsidy = GetBlockSubsidy(h, c);
    const CAmount res_sched = Consensus::GetResMintReward(h);
    BOOST_REQUIRE(subsidy > 0);
    BOOST_REQUIRE(res_sched > 0);

    auto run = [&](const RigorSupplyDeltaInput& in, bool expect_ok, const char* reject = nullptr) {
        BlockValidationState state;
        RigorSupplyDeltaResult out;
        const bool ok = RigorSupplyDelta(in, c, out, state);
        if (expect_ok) {
            BOOST_CHECK(ok);
            BOOST_CHECK(state.IsValid());
            BOOST_CHECK_EQUAL(out.oru_subsidy, subsidy);
            BOOST_CHECK_EQUAL(out.res_reward_scheduled, res_sched);
            BOOST_CHECK_EQUAL(out.oru_claimable, (in.fees_oru - in.fee_burn) + subsidy);
            BOOST_CHECK_EQUAL(out.oru_circulating_delta, subsidy - in.converge_sum);
            BOOST_CHECK_EQUAL(out.res_delta, in.res_mint);
        } else {
            BOOST_CHECK(!ok);
            BOOST_REQUIRE(reject != nullptr);
            BOOST_CHECK_EQUAL(state.GetRejectReason(), reject);
        }
        return out;
    };

    RigorSupplyDeltaInput none;
    none.height = h;
    none.oru_coinbase = subsidy;
    run(none, true);

    RigorSupplyDeltaInput converge = none;
    converge.converge_sum = 2 * COIN;
    run(converge, true);

    RigorSupplyDeltaInput burn = none;
    burn.fees_oru = 100 * COIN;
    burn.fee_burn = 10 * COIN;
    burn.oru_coinbase = subsidy + 90 * COIN;
    run(burn, true);

    RigorSupplyDeltaInput mint = none;
    mint.has_winner = true;
    mint.draw_cardinality = 3;
    mint.res_mint = res_sched;
    mint.res_coinbase = res_sched;
    run(mint, true);

    RigorSupplyDeltaInput combined = burn;
    combined.converge_sum = 2 * COIN;
    combined.has_winner = true;
    combined.draw_cardinality = 3;
    combined.res_mint = res_sched;
    combined.res_coinbase = res_sched;
    const RigorSupplyDeltaResult got = run(combined, true);
    BOOST_CHECK_EQUAL(got.oru_circulating_delta, subsidy - 2 * COIN);
    BOOST_CHECK_EQUAL(got.res_delta, res_sched);
    BOOST_CHECK_NE(got.oru_circulating_delta, subsidy);
    BOOST_CHECK_NE(got.res_delta, 0);

    RigorSupplyDeltaInput too_much = none;
    too_much.oru_coinbase = subsidy + 1;
    run(too_much, false, "bad-cb-amount");

    RigorSupplyDeltaInput no_winner_mint = none;
    no_winner_mint.draw_cardinality = Consensus::CONVERGENCE_DRAW_MIN_TXS_FOR_RES;
    no_winner_mint.res_mint = res_sched;
    no_winner_mint.res_coinbase = res_sched;
    run(no_winner_mint, false, "bad-rigor-res-without-winner");

    RigorSupplyDeltaInput underflow = none;
    underflow.draw_cardinality = 0;
    underflow.res_mint = res_sched;
    underflow.res_coinbase = res_sched;
    run(underflow, false, "bad-rigor-res-trigger-underflow");

    RigorSupplyDeltaInput winner_omit = none;
    winner_omit.has_winner = true;
    winner_omit.draw_cardinality = 1;
    run(winner_omit, false, "bad-rigor-res-winner-mismatch");
}

BOOST_AUTO_TEST_CASE(resolve_convergence_draw_is_pure_and_deterministic)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);

    ConvergenceDrawSet empty;
    empty.source_height = 400;
    empty.source_block_hash = uint256S("0x11");
    const ConvergenceDrawOutcome none = ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(!none.HasWinner());
    BOOST_CHECK(none.sigmas.empty());

    ConvergenceDrawSet set;
    set.source_height = 500;
    set.source_nbits = 0x207fffff;
    set.source_block_hash = uint256S("0xaa");
    set.candidates.push_back(ConvergenceDrawCandidate{
        uint256S("0x0000000000000000000000000000000000000000000000000000000000000011"),
        static_cast<uint64_t>(kSaturatingWeight), uint160{}});
    set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids({set.candidates[0].txid});

    const ConvergenceDrawOutcome a = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    const ConvergenceDrawOutcome b = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(a.HasWinner());
    BOOST_CHECK_EQUAL(a.winner_index, 0);
    BOOST_CHECK_EQUAL(a.winner_index, b.winner_index);
    BOOST_CHECK(a.sigmas == b.sigmas);
    BOOST_REQUIRE_EQUAL(a.sigmas.size(), 1U);
    BOOST_CHECK(Consensus::ConvergenceDrawInequalityHolds(
        a.sigmas[0], Consensus::RES_PROBABILITY_TARGET_CONST, kSaturatingWeight));

    const ConvergenceDrawOutcome t0 = ResolveConvergenceDraw(set, uint256());
    BOOST_CHECK(!t0.HasWinner());
    BOOST_REQUIRE_EQUAL(t0.eligible.size(), 1U);
    BOOST_CHECK(!t0.eligible[0]);

    ConvergenceDrawSet zero_w = set;
    zero_w.candidates[0].weight = 0;
    const ConvergenceDrawOutcome z = ResolveConvergenceDraw(zero_w, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(!z.HasWinner());
}

BOOST_AUTO_TEST_CASE(classify_rigor_asset_is_fail_closed)
{
    const CScript oru = OruP2wpkh();
    const CScript res = ResNative();
    const CScript mint = GetResMintScript(100, uint160());
    const CScript converge = GetConvergeScript(uint160());
    const CScript op_return = CScript() << RIGOR_MARKER_RETURN;
    const CScript empty;
    const CScript wrong_len = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(19, 0x11);
    const CScript unsupported = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript malformed_mint =
        CScript() << CScriptNum(0) << std::vector<unsigned char>(19, 0x11) << RIGOR_MARKER_RES_MINT;

    BOOST_CHECK_EQUAL(ClassifyRigorAsset(oru), ASSET_ORU);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(res), ASSET_RES);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(mint), ASSET_RES);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(converge), ASSET_ORU);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(op_return), ASSET_NONE);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(empty), ASSET_INVALID);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(wrong_len), ASSET_INVALID);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(unsupported), ASSET_INVALID);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(malformed_mint), ASSET_INVALID);

    for (const CScript& s : {oru, res, mint, converge, op_return, empty, wrong_len, unsupported, malformed_mint}) {
        BOOST_CHECK_EQUAL(ClassifyRigorAsset(s), AssetTypeFromScriptPubKey(s));
    }
}

BOOST_AUTO_TEST_CASE(coinbase_layout_budget_identity)
{
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(0, 0, 40 * Consensus::RES_COIN), 40 * Consensus::RES_COIN);
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(0, 5 * Consensus::RES_COIN, 40 * Consensus::RES_COIN), 45 * Consensus::RES_COIN);
}

BOOST_AUTO_TEST_CASE(p048_issuance_determinism_and_closed_sums)
{
    const auto& c = Params().GetConsensus();
    const RigorIssuance a = ComputeRigorIssuance(210000, c);
    const RigorIssuance b = ComputeRigorIssuance(210000, c);
    BOOST_CHECK_EQUAL(a.oru_subsidy, b.oru_subsidy);
    BOOST_CHECK_EQUAL(a.res_reward, b.res_reward);
    BOOST_CHECK_EQUAL(a.epoch, b.epoch);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(-5), 0);
    // int height < 0 is not a production input. GetBlockSubsidy only special-cases 0.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(-1, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);

    CAmount sum_209999 = 0;
    CAmount sum_210000 = 0;
    for (int h = 1; h <= 210000; ++h) {
        const CAmount r = GetBlockSubsidy(h, c);
        if (h <= 209999) sum_209999 += r;
        sum_210000 += r;
    }
    BOOST_CHECK_EQUAL(sum_209999 / COIN, 209999000);
    BOOST_CHECK_EQUAL(sum_210000 / COIN, 209999500);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(0, c).res_reward, Consensus::GetResMintReward(0));
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(0), Consensus::RES_REWARD_GENESIS);
}

BOOST_AUTO_TEST_CASE(p048_supply_delta_is_explicit_and_repeatable)
{
    const auto& c = Params().GetConsensus();
    RigorSupplyDeltaInput in;
    in.height = 1;
    in.fees_oru = 100 * COIN;
    in.fee_burn = 10 * COIN;
    in.converge_sum = 2 * COIN;
    in.has_winner = true;
    in.draw_cardinality = 2;
    in.res_mint = Consensus::GetResMintReward(1);
    in.res_coinbase = in.res_mint;
    in.oru_coinbase = GetBlockSubsidy(1, c) + 90 * COIN;

    BlockValidationState s1;
    BlockValidationState s2;
    RigorSupplyDeltaResult a;
    RigorSupplyDeltaResult b;
    BOOST_CHECK(RigorSupplyDelta(in, c, a, s1));
    BOOST_CHECK(RigorSupplyDelta(in, c, b, s2));
    BOOST_CHECK(s1.IsValid());
    BOOST_CHECK_EQUAL(a.oru_circulating_delta, b.oru_circulating_delta);
    BOOST_CHECK_EQUAL(a.res_delta, b.res_delta);
    BOOST_CHECK_EQUAL(a.oru_claimable, b.oru_claimable);
}

BOOST_AUTO_TEST_CASE(p048_draw_winner_is_canonical_not_scan_order)
{
    ConvergenceDrawSet set;
    set.source_height = 500;
    set.source_block_hash = uint256S("0xaa");
    set.candidates.push_back(ConvergenceDrawCandidate{
        uint256S("0x0000000000000000000000000000000000000000000000000000000000000011"),
        static_cast<uint64_t>(kSaturatingWeight), uint160{}});
    set.candidates.push_back(ConvergenceDrawCandidate{
        uint256S("0x0000000000000000000000000000000000000000000000000000000000000022"),
        static_cast<uint64_t>(kSaturatingWeight), uint160{}});
    set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids(
        {set.candidates[0].txid, set.candidates[1].txid});

    const ConvergenceDrawOutcome a = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_REQUIRE(a.HasWinner());
    const uint256 winner_txid = set.candidates[static_cast<size_t>(a.winner_index)].txid;
    std::swap(set.candidates[0], set.candidates[1]);
    const ConvergenceDrawOutcome b = ResolveConvergenceDraw(set, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_REQUIRE(b.HasWinner());
    BOOST_CHECK(set.candidates[static_cast<size_t>(b.winner_index)].txid == winner_txid);
}

BOOST_AUTO_TEST_CASE(p049_genesis_schedule_is_not_actual_issuance)
{
    const auto& c = Params().GetConsensus();
    const RigorIssuance g = ComputeRigorIssuance(0, c);
    BOOST_CHECK_EQUAL(g.epoch, 0);
    BOOST_CHECK_EQUAL(g.oru_subsidy, 0);
    BOOST_CHECK_EQUAL(g.res_reward, Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(0), Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_NE(Consensus::GetResMintReward(0), 0);

    RigorSupplyDeltaInput in;
    in.height = 0;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK_EQUAL(out.oru_subsidy, 0);
    BOOST_CHECK_EQUAL(out.res_reward_scheduled, Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, 0);
    BOOST_CHECK_EQUAL(out.res_delta, 0);
    BOOST_CHECK_EQUAL(out.oru_claimable, 0);
}

BOOST_AUTO_TEST_CASE(p049_genesis_draw_empty_f_is_none)
{
    ConvergenceDrawSet genesis_set;
    genesis_set.source_height = 0;
    BOOST_CHECK(genesis_set.IsSet());
    BOOST_CHECK(genesis_set.candidates.empty());
    const ConvergenceDrawOutcome o =
        ResolveConvergenceDraw(genesis_set, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(!o.HasWinner());
}

BOOST_AUTO_TEST_CASE(p049_maturity_and_epoch_pins)
{
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, 100);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nCoinbaseMaturity, 100);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(0), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(1), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210000), 1);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210001), 1);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 209999 / Consensus::RES_REWARD_EPOCH);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210000), 210000 / Consensus::RES_REWARD_EPOCH);
    BOOST_CHECK_EQUAL(1 / Consensus::RES_REWARD_EPOCH, 0);
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_GT(GetBlockSubsidy(1, c), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c) / COIN, 1000);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(209999), Consensus::GetResMintReward(0));
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(1), Consensus::GetResMintReward(0));
    BOOST_CHECK_NE(Consensus::GetResMintReward(210000), Consensus::GetResMintReward(209999));
    BOOST_CHECK_LT(100 - 1, COINBASE_MATURITY);
    BOOST_CHECK_GE(101 - 1, COINBASE_MATURITY);
}

BOOST_AUTO_TEST_SUITE_END()
