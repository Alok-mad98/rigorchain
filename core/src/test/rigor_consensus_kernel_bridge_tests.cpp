// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-052 Consensus Kernel ↔ Coq bridge pins. Not a refinement.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(rigor_consensus_kernel_bridge_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(p052_genesis_schedule_is_not_actual)
{
    const auto& c = Params().GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, c), 0);
    BOOST_CHECK_EQUAL(Consensus::GetResMintReward(0), Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(Consensus::RES_REWARD_GENESIS, 40 * Consensus::RES_COIN);
    BOOST_CHECK_NE(Consensus::GetResMintReward(0), 0);

    RigorSupplyDeltaInput in;
    in.height = 0;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    BOOST_CHECK_EQUAL(out.oru_subsidy, 0);
    BOOST_CHECK_EQUAL(out.res_reward_scheduled, Consensus::RES_REWARD_GENESIS);
    BOOST_CHECK_EQUAL(out.res_delta, 0);
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, 0);
}

BOOST_AUTO_TEST_CASE(p052_halving_epoch_boundary)
{
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(0), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(1), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(99), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 0);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210000), 1);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(210001), 1);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(419999), 1);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(420000), 2);
    BOOST_CHECK_EQUAL(RigorIssuanceEpoch(209999), 209999 / 210000);
    BOOST_CHECK_NE(RigorIssuanceEpoch(210000), 209999 / 210000);
}

BOOST_AUTO_TEST_CASE(p052_maturity_issued_is_not_mature)
{
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(RES_MINT_MATURITY, 100);
    BOOST_CHECK_LT(100 - 1, COINBASE_MATURITY);
    BOOST_CHECK_GE(101 - 1, COINBASE_MATURITY);
}

BOOST_AUTO_TEST_CASE(p052_empty_F_draw_none)
{
    ConvergenceDrawSet empty;
    empty.source_height = 0;
    BOOST_CHECK(empty.candidates.empty());
    const ConvergenceDrawOutcome o =
        ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_CHECK(!o.HasWinner());
}

BOOST_AUTO_TEST_CASE(p052_supply_layer_mapping)
{
    const auto& c = Params().GetConsensus();
    const CAmount subsidy = GetBlockSubsidy(1, c);
    RigorSupplyDeltaInput in;
    in.height = 1;
    in.fees_oru = 100;
    in.fee_burn = 5;
    in.converge_sum = 10;
    in.oru_coinbase = (100 - 5) + subsidy;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    // C++ circulating_delta is RFC-002 ledger: subsidy − converge.
    // Coq apply_delta circulating also subtracts burn. Do not unify.
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, subsidy - 10);
    BOOST_CHECK_EQUAL(out.oru_claimable, (100 - 5) + subsidy);
    BOOST_CHECK_NE(out.oru_circulating_delta, subsidy - 10 - 5);
}

BOOST_AUTO_TEST_CASE(p052_classify_authority_is_txout)
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    const CScript empty;
    std::vector<std::vector<unsigned char>> sols;
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(oru), AssetTypeFromScriptPubKey(oru));
    BOOST_CHECK_EQUAL(ClassifyRigorTxout(oru, sols), TxoutType::WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(empty), ASSET_INVALID);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(empty), ASSET_INVALID);
    BOOST_CHECK_EQUAL(ClassifyRigorTxout(empty, sols), TxoutType::NONSTANDARD);
}

BOOST_AUTO_TEST_SUITE_END()
