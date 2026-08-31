// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P069: testnet/regtest runtime canary. Does not switch production.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/params.h>
#include <node/kernel_runtime_canary.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <string>

struct KernelTestnetCanarySetup : public BasicTestingSetup {
    KernelTestnetCanarySetup() : BasicTestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_testnet_canary, KernelTestnetCanarySetup)

BOOST_AUTO_TEST_CASE(startup_flag_default_off)
{
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE(!gArgs.GetBoolArg("-enableconsensuskernelcanary", DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(1, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE(network_restriction_mainnet_armed)
{
    BOOST_REQUIRE(RigorKernelRuntimeCanaryNetworkAllowed(Params()));
    const auto testnet = CChainParams::TestNet();
    BOOST_REQUIRE(testnet);
    BOOST_REQUIRE(RigorKernelRuntimeCanaryNetworkAllowed(*testnet));
    const auto main = CChainParams::Main();
    BOOST_REQUIRE(main);
    BOOST_REQUIRE(RigorKernelRuntimeCanaryNetworkAllowed(*main));

    ArgsManager args;
    args.ForceSetArg("-enableconsensuskernelcanary", "1");
    std::string err;
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(args, *main, err));
    BOOST_REQUIRE(err.empty());

    err.clear();
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(args, Params(), err));
    BOOST_REQUIRE(err.empty());

    ArgsManager off;
    off.ForceSetArg("-enableconsensuskernelcanary", "0");
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(off, *main, err));
    BOOST_REQUIRE_EQUAL(std::string(KERNEL_RUNTIME_CANARY_MAINNET_NOT_ARMED), "MAINNET_CANARY_NOT_ARMED");
}

BOOST_AUTO_TEST_CASE(comparison_path_regtest_stream)
{
    RigorKernelRuntimeCanaryResetStats();
    RigorKernelRuntimeCanarySetLogDir("logs/consensus_kernel_canary");
    std::string err;
    BOOST_REQUIRE_MESSAGE(RigorKernelRuntimeCanaryRunBlockStream(10000, err), err);
    BOOST_REQUIRE_EQUAL(RigorKernelRuntimeCanaryDiscrepancyCount(), 0);
    BOOST_REQUIRE_EQUAL(RigorKernelRuntimeCanaryCrashCount(), 0);
    BOOST_REQUIRE_EQUAL(RigorKernelRuntimeCanaryRejectDivergence(), 0);
    BOOST_REQUIRE_EQUAL(RigorKernelRuntimeCanaryBlocksValidated(), 10000);
}

BOOST_AUTO_TEST_CASE(failure_handling_no_autorepair)
{
    RigorKernelRuntimeCanaryResetStats();
    std::string err;
    BOOST_REQUIRE(RigorKernelRuntimeCanaryRunBlockStream(1, err));
    BOOST_REQUIRE(err.empty());
    BOOST_REQUIRE_EQUAL(std::string(KERNEL_RUNTIME_CANARY_DISCREPANCY),
        "CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY");
    BOOST_REQUIRE(!std::string(KERNEL_RUNTIME_CANARY_MAINNET_FORBIDDEN).empty());
}

BOOST_AUTO_TEST_SUITE_END()
