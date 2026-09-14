// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-047 Consensus Kernel first cut. Production schedules are not redefined.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/rigor_coinbase_layout.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
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

#include <vector>

namespace {

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

BOOST_AUTO_TEST_CASE(issuance_height_epoch_oru_closed)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(Consensus::RES_REWARD_EPOCH, 210000);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210000), 1);

    const int heights[] = {0, 1, 209999, 210000, 210001, 419999, 420000};
    for (int h : heights) {
        const RigorIssuance got = ComputeRigorIssuance(h, c);
        BOOST_CHECK_EQUAL(got.epoch, RigorIssuanceEpoch(h));
        BOOST_CHECK_EQUAL(got.oru_subsidy, GetBlockSubsidy(h, c));
    }

    BOOST_CHECK_EQUAL(ComputeRigorIssuance(0, c).oru_subsidy, 0);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(1, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(209999, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(210000, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(210001, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(419999, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(420000, c).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);

    const RigorIssuance last0 = ComputeRigorIssuance(209999, c);
    const RigorIssuance first1 = ComputeRigorIssuance(210000, c);
    BOOST_CHECK_EQUAL(last0.epoch, 0);
    BOOST_CHECK_EQUAL(first1.epoch, 1);
    BOOST_CHECK_EQUAL(first1.oru_subsidy, last0.oru_subsidy);
}

BOOST_AUTO_TEST_CASE(supply_delta_paths_are_not_degenerate)
{
    const auto& c = Params().GetConsensus();
    const int h = 1;
    const CAmount subsidy = GetBlockSubsidy(h, c);
    const CAmount res_body_mint = 40 * Consensus::RES_COIN;
    BOOST_REQUIRE(subsidy > 0);

    auto run = [&](const RigorSupplyDeltaInput& in, bool expect_ok, const char* reject = nullptr) {
        BlockValidationState state;
        RigorSupplyDeltaResult out;
        const bool ok = RigorSupplyDelta(in, c, out, state);
        if (expect_ok) {
            BOOST_CHECK(ok);
            BOOST_CHECK(state.IsValid());
            BOOST_CHECK_EQUAL(out.oru_subsidy, subsidy);
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
    mint.res_mint = res_body_mint;
    run(mint, true);

    RigorSupplyDeltaInput combined = burn;
    combined.converge_sum = 2 * COIN;
    combined.res_mint = res_body_mint;
    const RigorSupplyDeltaResult got = run(combined, true);
    BOOST_CHECK_EQUAL(got.oru_circulating_delta, subsidy - 2 * COIN);
    BOOST_CHECK_EQUAL(got.res_delta, res_body_mint);
    BOOST_CHECK_NE(got.oru_circulating_delta, subsidy);
    BOOST_CHECK_NE(got.res_delta, 0);

    RigorSupplyDeltaInput too_much = none;
    too_much.oru_coinbase = subsidy + 1;
    run(too_much, false, "bad-cb-amount");
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
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(0, 0), 0);
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(0, 40 * Consensus::RES_COIN), 40 * Consensus::RES_COIN);
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(5 * Consensus::RES_COIN, 40 * Consensus::RES_COIN), 45 * Consensus::RES_COIN);
}

BOOST_AUTO_TEST_CASE(p048_issuance_determinism_and_closed_sums)
{
    const auto& c = Params().GetConsensus();
    const RigorIssuance a = ComputeRigorIssuance(210000, c);
    const RigorIssuance b = ComputeRigorIssuance(210000, c);
    BOOST_CHECK_EQUAL(a.oru_subsidy, b.oru_subsidy);
    BOOST_CHECK_EQUAL(a.epoch, b.epoch);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(-5), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(-1, c), 0);

    CAmount sum_209999 = 0;
    CAmount sum_210000 = 0;
    for (int h = 1; h <= 210000; ++h) {
        const CAmount r = GetBlockSubsidy(h, c);
        if (h <= 209999) sum_209999 += r;
        sum_210000 += r;
    }
    BOOST_CHECK_EQUAL(sum_209999, 209999 * Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_CHECK_EQUAL(sum_210000, 210000 * Consensus::ORU_FLAT_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(p048_supply_delta_is_explicit_and_repeatable)
{
    const auto& c = Params().GetConsensus();
    RigorSupplyDeltaInput in;
    in.height = 1;
    in.fees_oru = 100 * COIN;
    in.fee_burn = 10 * COIN;
    in.converge_sum = 2 * COIN;
    in.res_mint = 40 * Consensus::RES_COIN;
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

BOOST_AUTO_TEST_CASE(p049_genesis_schedule_is_not_actual_issuance)
{
    const auto& c = Params().GetConsensus();
    const RigorIssuance g = ComputeRigorIssuance(0, c);
    BOOST_CHECK_EQUAL(g.epoch, 0);
    BOOST_CHECK_EQUAL(g.oru_subsidy, 0);

    RigorSupplyDeltaInput in;
    in.height = 0;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK_EQUAL(out.oru_subsidy, 0);
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, 0);
    BOOST_CHECK_EQUAL(out.res_delta, 0);
    BOOST_CHECK_EQUAL(out.oru_claimable, 0);
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
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, c) / COIN, 128);
    BOOST_CHECK_LT(100 - 1, COINBASE_MATURITY);
    BOOST_CHECK_GE(101 - 1, COINBASE_MATURITY);
}

BOOST_AUTO_TEST_SUITE_END()
