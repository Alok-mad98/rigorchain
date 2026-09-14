// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-054: single production Kernel entry. Not a refinement. Not a switch.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#ifdef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "production test_rigor must not compile the C ABI verify-only adapter"
#endif

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_single_authority, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(single_entry_issuance)
{
    const auto& c = Params().GetConsensus();
    const int heights[] = {0, 1, 99, 100, 101, 209999, 210000, 419999, 420000};
    for (int h : heights) {
        const RigorIssuance got = ComputeRigorIssuance(h, c);
        BOOST_CHECK_EQUAL(got.epoch, RigorIssuanceEpoch(h));
        BOOST_CHECK_EQUAL(got.oru_subsidy, GetBlockSubsidy(h, c));
    }
    BOOST_CHECK_EQUAL(ComputeRigorIssuance(0, c).oru_subsidy, 0);
}

BOOST_AUTO_TEST_CASE(single_entry_classify)
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    const CScript empty;
    std::vector<std::vector<unsigned char>> sols;
    BOOST_CHECK_EQUAL(ClassifyRigorTxout(oru, sols), TxoutType::WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(oru), ASSET_ORU);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(oru), AssetTypeFromScriptPubKey(oru));
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(res), ASSET_RES);
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(res), AssetTypeFromScriptPubKey(res));
    BOOST_CHECK_EQUAL(ClassifyRigorAsset(empty), ASSET_INVALID);
    BOOST_CHECK_EQUAL(Solver(oru, sols), ClassifyRigorTxout(oru, sols));
}

BOOST_AUTO_TEST_CASE(single_entry_supply)
{
    const auto& c = Params().GetConsensus();
    const CAmount subsidy = GetBlockSubsidy(1, c);
    RigorSupplyDeltaInput in;
    in.height = 1;
    in.fees_oru = 100;
    in.fee_burn = 5;
    in.converge_sum = 10;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    BOOST_CHECK_EQUAL(out.oru_subsidy, subsidy);
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, subsidy - 10);
    BOOST_CHECK_EQUAL(out.oru_claimable, (100 - 5) + subsidy);
    BOOST_CHECK_NE(out.oru_circulating_delta, subsidy - 10 - 5);
}

BOOST_AUTO_TEST_CASE(connectblock_orchestration_genesis)
{
    const auto& c = Params().GetConsensus();
    RigorSupplyDeltaInput in;
    in.height = 0;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_CHECK(RigorSupplyDelta(in, c, out, state));
    BOOST_CHECK_EQUAL(out.oru_subsidy, 0);
    BOOST_CHECK_EQUAL(out.oru_circulating_delta, 0);
    BOOST_CHECK_EQUAL(out.res_delta, 0);
}

BOOST_AUTO_TEST_SUITE_END()
