// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P073: genesis Consensus Kernel is the unique production authority.
// Does not recode formulas. EQUIVALENCE_PROVED remains false.

#include <consensus/kernel/genesis_authority.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

static CTransactionRef EmptyCoinbase()
{
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vout.resize(1);
    tx.vout[0].nValue = 0;
    tx.vout[0].scriptPubKey = CScript();
    return MakeTransactionRef(tx);
}

BOOST_FIXTURE_TEST_SUITE(test_genesis_consensus_authority, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(switched_constants)
{
    BOOST_REQUIRE_EQUAL(PRODUCTION_KERNEL_SWITCHED, true);
    BOOST_REQUIRE_EQUAL(GENESIS_SINGLE_PRODUCTION_AUTHORITY, true);
    BOOST_REQUIRE_EQUAL(GENESIS_EQUIVALENCE_PROVED, false);
}

BOOST_AUTO_TEST_CASE(genesis_height_0)
{
    const Consensus::Params p = FrozenParams();
    const RigorIssuance g = RigorConsensusKernel::Issuance(0, p);
    BOOST_REQUIRE_EQUAL(g.oru_subsidy, 0);
    BOOST_REQUIRE_EQUAL(g.res_reward, Consensus::RES_REWARD_GENESIS);
    BOOST_REQUIRE(!RigorConsensusKernel::ResolveDraw(ConvergenceDrawSet{}, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner());

    RigorConsensusKernelConnectInput in;
    in.height = 0;
    const CTransactionRef cb = EmptyCoinbase();
    in.coinbase = cb.get();
    in.params = &p;
    RigorConsensusKernelConnectResult out;
    BlockValidationState state;
    BOOST_REQUIRE(RigorConsensusKernel::EvaluateConnect(in, out, state));
    BOOST_REQUIRE(out.accept);
    BOOST_REQUIRE_EQUAL(out.res_mint, 0);
    BOOST_REQUIRE_EQUAL(out.supply.oru_circulating_delta, 0);
    BOOST_REQUIRE_EQUAL(out.supply.res_delta, 0);
}

BOOST_AUTO_TEST_CASE(height_1_issuance)
{
    const Consensus::Params p = FrozenParams();
    const RigorIssuance g = RigorConsensusKernel::Issuance(1, p);
    BOOST_REQUIRE_EQUAL(g.oru_subsidy, 100000000000LL);
    BOOST_REQUIRE_EQUAL(g.res_reward, 40000000);
    BOOST_REQUIRE_EQUAL(g.epoch, 0);
}

BOOST_AUTO_TEST_CASE(maturity)
{
    const Consensus::Params p = FrozenParams();
    BOOST_REQUIRE(!RigorConsensusKernel::CoinbaseMature(1, 1, p.nCoinbaseMaturity));
    BOOST_REQUIRE(!RigorConsensusKernel::CoinbaseMature(100, 1, p.nCoinbaseMaturity));
    BOOST_REQUIRE(RigorConsensusKernel::CoinbaseMature(101, 1, p.nCoinbaseMaturity));
}

BOOST_AUTO_TEST_CASE(epoch_boundary)
{
    const Consensus::Params p = FrozenParams();
    BOOST_REQUIRE_EQUAL(RigorIssuanceEpoch(209999), 0);
    BOOST_REQUIRE_EQUAL(RigorIssuanceEpoch(210000), 1);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(209999, p).oru_subsidy, 100000000000LL);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(210000, p).oru_subsidy, 50000000000LL);
}

BOOST_AUTO_TEST_CASE(res_schedule)
{
    const Consensus::Params p = FrozenParams();
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(209999, p).res_reward, 40000000);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(210000, p).res_reward, 14142196);
}

BOOST_AUTO_TEST_CASE(draw)
{
    const uint64_t sat = 209715200000000ULL;
    ConvergenceDrawSet one;
    one.source_height = 100;
    ConvergenceDrawCandidate a, b;
    a.txid = uint256S("0x01");
    a.weight = sat;
    b.txid = uint256S("0x02");
    b.weight = sat;
    one.candidates = {a};
    BOOST_REQUIRE(RigorConsensusKernel::ResolveDraw(one, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner());
    ConvergenceDrawSet two = one;
    two.candidates = {a, b};
    ConvergenceDrawSet swap = one;
    swap.candidates = {b, a};
    const auto w2 = RigorConsensusKernel::ResolveDraw(two, Consensus::RES_PROBABILITY_TARGET_CONST);
    const auto w3 = RigorConsensusKernel::ResolveDraw(swap, Consensus::RES_PROBABILITY_TARGET_CONST);
    BOOST_REQUIRE(w2.HasWinner() && w3.HasWinner());
    BOOST_REQUIRE(two.candidates[static_cast<size_t>(w2.winner_index)].txid ==
        swap.candidates[static_cast<size_t>(w3.winner_index)].txid);
    BOOST_REQUIRE(!RigorConsensusKernel::ResolveDraw(one, uint256()).HasWinner());
}

BOOST_AUTO_TEST_CASE(classification)
{
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Classify(oru), ASSET_ORU);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Classify(res), ASSET_RES);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Classify(CScript()), ASSET_INVALID);
}

BOOST_AUTO_TEST_CASE(spend_and_ledger_transition)
{
    const Consensus::Params p = FrozenParams();
    const CTransactionRef cb = EmptyCoinbase();
    RigorConsensusKernelConnectInput in;
    in.height = 1;
    in.coinbase = cb.get();
    in.params = &p;
    in.supply.oru_coinbase = 0;
    RigorConsensusKernelConnectResult out;
    BlockValidationState state;
    BOOST_REQUIRE(RigorConsensusKernel::EvaluateConnect(in, out, state));
    BOOST_REQUIRE(out.accept);
    BOOST_REQUIRE_EQUAL(out.res_mint, 0);
}

BOOST_AUTO_TEST_SUITE_END()
