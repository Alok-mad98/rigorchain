// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P072: long-run mainnet canary observation. Does not switch production.
// Default BasicTestingSetup is mainnet. 100000-block soak lives in the
// audit driver, not in test_rigor.

#include <chainparams.h>
#include <consensus/kernel_canary/MainnetCanaryMonitor.h>
#include <consensus/kernel_canary/canary_controller.h>
#include <consensus/kernel_canary/canary_health.h>
#include <test/kernel_runtime_canary.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_mainnet_canary_longrun, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(disabled_by_default)
{
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(!Params().IsTestChain());
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE(!gArgs.GetBoolArg("-enableconsensuskernelcanary", DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY));
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
    BOOST_REQUIRE_EQUAL(std::string(MainnetCanaryMonitor::Instance().Status()), "OFF");
    BOOST_REQUIRE_EQUAL(MainnetCanaryMonitor::Instance().Health(), ConsensusKernelCanaryHealthState::PAUSED);
    BOOST_REQUIRE_EQUAL(CANARY_FAULT_INJECTION_TEST, false);
    BOOST_REQUIRE_EQUAL(MAINNET_CANARY_LONGRUN_BLOCKS, 100000);
}

BOOST_AUTO_TEST_CASE(armed_operator_mode)
{
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(!PRODUCTION_MAINNET_CANARY_ARMED);
    auto& ctl = ConsensusKernelCanaryController::Instance();
    std::string err;
    BOOST_REQUIRE(!ctl.Enable(false, true, err));
    err.clear();
    BOOST_REQUIRE(!ctl.Enable(true, true, err));
    BOOST_REQUIRE_EQUAL(err, KERNEL_CANARY_MAINNET_NOT_ARMED);
    BOOST_REQUIRE(!ctl.Enabled());

    auto& mon = MainnetCanaryMonitor::Instance();
    BOOST_REQUIRE(!mon.Start());
    BOOST_REQUIRE(!mon.enabled());

    struct RestoreCanaryArg {
        ~RestoreCanaryArg() { gArgs.ForceSetArg("-enableconsensuskernelcanary", "0"); }
    } restore;
    gArgs.ForceSetArg("-enableconsensuskernelcanary", "1");
    BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
}

BOOST_AUTO_TEST_CASE(normal_compare)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(!mon.Start());
    std::string err;
    BOOST_REQUIRE(!mon.RunSoak(2000, err));
    BOOST_REQUIRE_EQUAL(err, KERNEL_MAINNET_CANARY_NOT_ARMED);
}

BOOST_AUTO_TEST_CASE(fault_injection)
{
    static const char* kFaults[] = {"issuance", "draw", "classify", "ledger_delta"};
    for (const char* name : kFaults) {
        auto& mon = MainnetCanaryMonitor::Instance();
        RigorKernelRuntimeCanaryResetStats();
        BOOST_REQUIRE(!mon.Start());
        BOOST_REQUIRE(mon.FaultInject(name));
        BOOST_REQUIRE(!mon.enabled());
        BOOST_REQUIRE_EQUAL(mon.Health(), ConsensusKernelCanaryHealthState::DISABLED);
        BOOST_REQUIRE_EQUAL(ConsensusKernelCanaryController::Instance().LastAuthority(), name);
        BOOST_REQUIRE_EQUAL(ConsensusKernelCanaryController::Instance().LastReason(),
            KERNEL_MAINNET_CANARY_DISCREPANCY);
        BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(101, uint256(), uint256(), {}, uint256(), {}, {},
            Params().GetConsensus()));
        BOOST_REQUIRE(!RigorKernelRuntimeCanaryEnabled());
    }
}

BOOST_AUTO_TEST_CASE(rollback)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(!mon.Start());
    const uint64_t before = mon.rollback_events();
    mon.DisableOnFailure();
    BOOST_REQUIRE(!mon.enabled());
    BOOST_REQUIRE_EQUAL(mon.Health(), ConsensusKernelCanaryHealthState::DISABLED);
    BOOST_REQUIRE(mon.rollback_events() > before);
    BOOST_REQUIRE(!ConsensusKernelCanaryController::Instance().CompareAllowed());
}

BOOST_AUTO_TEST_CASE(continue_cpp_authority)
{
    auto& mon = MainnetCanaryMonitor::Instance();
    RigorKernelRuntimeCanaryResetStats();
    BOOST_REQUIRE(!mon.Start());
    mon.InjectDiscrepancyForTest(210000, "bh", "txid", "cpp", "cand", "issuance");
    BOOST_REQUIRE(RigorObserveConnectBlockRuntimeCanary(210000, uint256(), uint256(), {}, uint256(), {}, {},
        Params().GetConsensus()));
    BOOST_REQUIRE_EQUAL(DEFAULT_ENABLE_CONSENSUS_KERNEL_CANARY, false);
    BOOST_REQUIRE_EQUAL(PRODUCTION_MAINNET_CANARY_ARMED, false);
}

BOOST_AUTO_TEST_SUITE_END()
