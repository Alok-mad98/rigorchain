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

#include <boost/test/unit_test.hpp>

#include <string>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
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
    BOOST_REQUIRE_EQUAL(std::string(PRODUCTION_CONSENSUS_AUTHORITY), "RigorConsensusKernel(C++)");
    BOOST_REQUIRE_EQUAL(EXTRACTED_KERNEL_PRODUCTION, false);
    BOOST_REQUIRE_EQUAL(COMPCERT_BINARY_VERIFIED_KERNEL, false);
    BOOST_REQUIRE_EQUAL(GENESIS_SINGLE_PRODUCTION_AUTHORITY, true);
    BOOST_REQUIRE_EQUAL(GENESIS_EQUIVALENCE_PROVED, false);
}

BOOST_AUTO_TEST_CASE(genesis_height_0)
{
    const Consensus::Params p = FrozenParams();
    const RigorIssuance g = RigorConsensusKernel::Issuance(0, p);
    BOOST_REQUIRE_EQUAL(g.oru_subsidy, 0);

    RigorConsensusKernelConnectInput in;
    in.height = 0;
    const CTransactionRef cb = EmptyCoinbase();
    in.coinbase = cb.get();
    in.params = &p;
    RigorConsensusKernelConnectResult out;
    BlockValidationState state;
    BOOST_REQUIRE(RigorConsensusKernel::EvaluateConnect(in, out, state));
    BOOST_REQUIRE(out.accept);
    BOOST_REQUIRE_EQUAL(out.supply.oru_circulating_delta, 0);
    BOOST_REQUIRE_EQUAL(out.supply.res_delta, 0);
}

BOOST_AUTO_TEST_CASE(height_1_issuance)
{
    const Consensus::Params p = FrozenParams();
    const RigorIssuance g = RigorConsensusKernel::Issuance(1, p);
    BOOST_REQUIRE_EQUAL(g.oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
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
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(209999, p).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
    BOOST_REQUIRE_EQUAL(RigorConsensusKernel::Issuance(210000, p).oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
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
    BOOST_REQUIRE_EQUAL(out.supply.oru_subsidy, Consensus::ORU_FLAT_BLOCK_SUBSIDY);
}

BOOST_AUTO_TEST_CASE(evaluate_connect_rejects_null_input)
{
    RigorConsensusKernelConnectInput in;
    RigorConsensusKernelConnectResult out;
    BlockValidationState state;
    BOOST_REQUIRE(!RigorConsensusKernel::EvaluateConnect(in, out, state));
    BOOST_REQUIRE_EQUAL(state.GetRejectReason(), "kernel-connect-input");
}

BOOST_AUTO_TEST_CASE(evaluate_connect_rejects_negative_height)
{
    const Consensus::Params p = FrozenParams();
    const CTransactionRef cb = EmptyCoinbase();
    RigorConsensusKernelConnectInput in;
    in.height = -1;
    in.coinbase = cb.get();
    in.params = &p;
    RigorConsensusKernelConnectResult out;
    BlockValidationState state;
    BOOST_REQUIRE(!RigorConsensusKernel::EvaluateConnect(in, out, state));
    BOOST_REQUIRE_EQUAL(state.GetRejectReason(), "kernel-negative-height");
}

BOOST_AUTO_TEST_SUITE_END()
