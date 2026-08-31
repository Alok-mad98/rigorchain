// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P071: mainnet canary compare-only. Does not switch production.
// Default BasicTestingSetup is mainnet.

#include <chainparams.h>
#include <consensus/kernel_canary/MainnetCanaryMonitor.h>
#include <consensus/kernel_canary/canary_controller.h>
#include <node/kernel_runtime_canary.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_mainnet_canary, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(startup_disabled)
{
    BOOST_REQUIRE(!Params().IsTestChain());
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, true);
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE(!gArgs.GetBoolArg("-enableconsensuskernelcanary", DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
    BOOST_REQUIRE_EQUAL(std::string(MainnetCanaryMonitor::Instance().Status()), "OFF");
    BOOST_REQUIRE(!MainnetCanaryMonitor::Instance().enabled());
}

BOOST_AUTO_TEST_CASE(armed_enable)
{
    BOOST_REQUIRE(PRODUCTION_MAINNET_CANARY_ARMED);
    BOOST_REQUIRE(RigorKernelRuntimeCanaryNetworkAllowed(Params()));

    ArgsManager off;
    std::string err;
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(off, Params(), err));
    BOOST_REQUIRE(err.empty());

    ArgsManager args;
    args.ForceSetArg("-enableconsensuskernelcanary", "1");
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(args, Params(), err));
    BOOST_REQUIRE(err.empty());
    BOOST_REQUIRE_EQUAL(std::string(KERNEL_MAINNET_CANARY_NOT_ARMED), "MAINNET_CANARY_NOT_ARMED");

    auto& ctl = ConsensusKernelCanaryController::Instance();
    ctl.Reset();
    BOOST_REQUIRE(!ctl.Enable(false, true, err));
    BOOST_REQUIRE_EQUAL(err, KERNEL_CANARY_OPERATOR_EXPLICIT_REQUIRED);
    err.clear();
    BOOST_REQUIRE(ctl.Enable(true, true, err));
    BOOST_REQUIRE(ctl.Enabled());

    auto& mon = MainnetCanaryMonitor::Instance();
    mon.Reset();
    BOOST_REQUIRE(mon.Start());
    BOOST_REQUIRE(mon.enabled());
    BOOST_REQUIRE_EQUAL(std::string(mon.Status()), "ON");
    mon.Stop();
    BOOST_REQUIRE_EQUAL(std::string(mon.Status()), "OFF");
}

BOOST_AUTO_TEST_CASE(discrepancy_handling)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    auto& ctl = ConsensusKernelCanaryController::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(mon.Start());
    mon.InjectDiscrepancyForTest(210000, "bh", "txid", "cpp", "cand", "issuance");
    BOOST_REQUIRE(!mon.enabled());
    BOOST_REQUIRE_EQUAL(std::string(mon.Status()), "DISABLED");
    BOOST_REQUIRE(mon.discrepancy_count() >= 1);
    BOOST_REQUIRE_EQUAL(ctl.LastHeight(), 210000);
    BOOST_REQUIRE_EQUAL(ctl.LastBlockHash(), "bh");
    BOOST_REQUIRE_EQUAL(ctl.LastTxid(), "txid");
    BOOST_REQUIRE_EQUAL(ctl.LastCppResult(), "cpp");
    BOOST_REQUIRE_EQUAL(ctl.LastCandidateResult(), "cand");
    BOOST_REQUIRE_EQUAL(ctl.LastAuthority(), "issuance");
    BOOST_REQUIRE_EQUAL(ctl.LastReason(), KERNEL_MAINNET_CANARY_DISCREPANCY);
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(210000, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE(rollback)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(mon.Start());
    mon.DisableOnFailure();
    BOOST_REQUIRE(!mon.enabled());
    BOOST_REQUIRE(!ConsensusKernelCanaryController::Instance().CompareAllowed());
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(1, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
}

BOOST_AUTO_TEST_CASE(metrics)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(mon.Start());
    std::string err;
    BOOST_REQUIRE_MESSAGE(mon.RunSoak(10000, err), err);
    BOOST_REQUIRE_EQUAL(mon.discrepancy_count(), 0);
    BOOST_REQUIRE(mon.blocks_checked() >= 10000);
    BOOST_REQUIRE(mon.comparison_count() >= 10000);
    BOOST_REQUIRE(mon.mainnet_canary_blocks_total() >= 10000);
    BOOST_REQUIRE(mon.mainnet_canary_compare_total() >= 10000);
    BOOST_REQUIRE_EQUAL(mon.mainnet_canary_discrepancy_total(), 0);
    (void)mon.mainnet_canary_runtime_cpp();
    (void)mon.mainnet_canary_runtime_candidate();
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, true);
}

BOOST_AUTO_TEST_SUITE_END()
