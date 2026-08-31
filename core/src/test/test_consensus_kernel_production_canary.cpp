// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P070: production canary readiness. Does not switch production.
// Default BasicTestingSetup is mainnet.

#include <chainparams.h>
#include <consensus/kernel_canary/canary_controller.h>
#include <consensus/kernel_canary/canary_metrics.h>
#include <node/kernel_runtime_canary.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_production_canary, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mainnet_default_off)
{
    BOOST_REQUIRE(!Params().IsTestChain());
    BOOST_REQUIRE_EQUAL(CANARY_AVAILABLE, true);
    BOOST_REQUIRE_EQUAL(CANARY_ENABLED_DEFAULT, false);
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, true);
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE(!gArgs.GetBoolArg("-enableconsensuskernelcanary", DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY));
    BOOST_REQUIRE(RigorKernelRuntimeCanaryNetworkAllowed(Params()));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
    ConsensusKernelCanaryController::Instance().Reset();
    BOOST_REQUIRE(!ConsensusKernelCanaryController::Instance().Enabled());
    BOOST_REQUIRE_EQUAL(std::string(ConsensusKernelCanaryController::Instance().Status()), "OFF");
}

BOOST_AUTO_TEST_CASE(mainnet_explicit_enable_policy)
{
    ArgsManager args;
    args.ForceSetArg("-enableconsensuskernelcanary", "1");
    std::string err;
    BOOST_REQUIRE(RigorKernelRuntimeCanaryStartupCheck(args, Params(), err));
    BOOST_REQUIRE(err.empty());

    auto& ctl = ConsensusKernelCanaryController::Instance();
    ctl.Reset();
    BOOST_REQUIRE(!ctl.Enable(false, true, err));
    BOOST_REQUIRE_EQUAL(err, KERNEL_CANARY_OPERATOR_EXPLICIT_REQUIRED);
    BOOST_REQUIRE(!ctl.Enabled());

    err.clear();
    BOOST_REQUIRE(ctl.Enable(true, true, err));
    BOOST_REQUIRE(err.empty());
    BOOST_REQUIRE(ctl.Enabled());
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, true);
    ctl.Disable("rollback-test");
    BOOST_REQUIRE(!ctl.Enabled());
    BOOST_REQUIRE(!ctl.CompareAllowed());
}

BOOST_AUTO_TEST_CASE(rollback)
{
    auto& ctl = ConsensusKernelCanaryController::Instance();
    ctl.Reset();
    std::string err;
    BOOST_REQUIRE(ctl.Enable(true, false, err));
    BOOST_REQUIRE(ctl.CompareAllowed());
    ctl.Disable("operator-rollback");
    BOOST_REQUIRE(!ctl.CompareAllowed());
    BOOST_REQUIRE_EQUAL(std::string(ctl.Status()), "DISABLED");
    BOOST_REQUIRE_EQUAL(ctl.LastReason(), "operator-rollback");
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(1, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
}

BOOST_AUTO_TEST_CASE(discrepancy_handling)
{
    auto& ctl = ConsensusKernelCanaryController::Instance();
    ctl.Reset();
    auto& metrics = ConsensusKernelCanaryMetrics::Instance();
    metrics.Reset();
    ctl.OnDiscrepancy(210000, "bh", "txid", "cpp-accept", "cand-reject");
    BOOST_REQUIRE(!ctl.CompareAllowed());
    BOOST_REQUIRE_EQUAL(ctl.LastHeight(), 210000);
    BOOST_REQUIRE_EQUAL(ctl.LastBlockHash(), "bh");
    BOOST_REQUIRE_EQUAL(ctl.LastTxid(), "txid");
    BOOST_REQUIRE_EQUAL(ctl.LastCppResult(), "cpp-accept");
    BOOST_REQUIRE_EQUAL(ctl.LastCandidateResult(), "cand-reject");
    BOOST_REQUIRE_EQUAL(ctl.LastReason(), KERNEL_CANARY_ROLLBACK_DISCREPANCY);
    BOOST_REQUIRE_EQUAL(std::string(KERNEL_CANARY_ROLLBACK_DISCREPANCY),
        "CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY");
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(210000, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
}

BOOST_AUTO_TEST_CASE(no_consensus_mutation)
{
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(0, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(1, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
    BOOST_REQUIRE_EQUAL(RigorKernelRuntimeCanaryDiscrepancyCount(), 0);
    BOOST_REQUIRE_EQUAL(ConsensusKernelCanaryMetrics::Instance().comparisons.load(), 0);
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, true);
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
}

BOOST_AUTO_TEST_SUITE_END()
