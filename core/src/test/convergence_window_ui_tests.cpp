// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <consensus/rigor_consensus_lock.h>
#include <wallet/convergence_window_ui.h>

BOOST_AUTO_TEST_SUITE(convergence_window_ui_tests)

BOOST_AUTO_TEST_CASE(window_width_locked_to_32)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
    const auto s = wallet::BuildConvergenceUiWindow(/*inclusion=*/100, /*tip=*/100, false, false);
    BOOST_CHECK_EQUAL(s.W, 32);
    BOOST_CHECK_EQUAL(s.finalization_block, 132);
}

BOOST_AUTO_TEST_CASE(submitted_before_inclusion)
{
    const auto s = wallet::BuildConvergenceUiWindow(-1, 500, false, false);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::Submitted);
    BOOST_CHECK_EQUAL(s.start_block, -1);
}

BOOST_AUTO_TEST_CASE(active_window_block_countdown)
{
    // Example from GUI readiness task: start 850000, current 850017, finalization 850032.
    const auto s = wallet::BuildConvergenceUiWindow(850000, 850017, false, false);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow);
    BOOST_CHECK_EQUAL(s.elapsed_in_window, 17);
    BOOST_CHECK_EQUAL(s.remaining_blocks, 15);
    BOOST_CHECK_EQUAL(s.finalization_block, 850032);
}

BOOST_AUTO_TEST_CASE(finalization_pending_last_three_blocks)
{
    const auto s = wallet::BuildConvergenceUiWindow(1000, 1030, false, false); // remaining = 2
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::FinalizationPending);
    BOOST_CHECK_EQUAL(s.remaining_blocks, 2);
}

BOOST_AUTO_TEST_CASE(completed_allocated)
{
    const auto s = wallet::BuildConvergenceUiWindow(1000, 1010, true, false);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::CompletedAllocated);
    BOOST_CHECK(s.verification_pass);
}

BOOST_AUTO_TEST_CASE(completed_no_allocation_at_finalization)
{
    const auto s = wallet::BuildConvergenceUiWindow(1000, 1032, false, false);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::CompletedNoAllocation);
    BOOST_CHECK(s.verification_pass);
    BOOST_CHECK_EQUAL(s.remaining_blocks, 0);
}

BOOST_AUTO_TEST_CASE(premature_lost_does_not_force_complete)
{
    // Regression: wallet once marked LOST at inclusion; UI must keep W progress live.
    // Start 136, tip 156, finalization 168 → elapsed 20, remaining 12 (not 32/32).
    const auto s = wallet::BuildConvergenceUiWindow(/*inclusion=*/136, /*tip=*/156, false,
                                                   /*explicit_loss=*/true);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::ActiveWindow);
    BOOST_CHECK_EQUAL(s.elapsed_in_window, 20);
    BOOST_CHECK_EQUAL(s.remaining_blocks, 12);
    BOOST_CHECK_EQUAL(s.finalization_block, 168);
}

BOOST_AUTO_TEST_CASE(lost_after_window_is_complete)
{
    const auto s = wallet::BuildConvergenceUiWindow(/*inclusion=*/136, /*tip=*/168, false,
                                                   /*explicit_loss=*/true);
    BOOST_CHECK(s.lifecycle == wallet::ConvergenceUiLifecycle::CompletedNoAllocation);
    BOOST_CHECK_EQUAL(s.elapsed_in_window, 32);
    BOOST_CHECK_EQUAL(s.remaining_blocks, 0);
}

BOOST_AUTO_TEST_CASE(countdown_is_height_based_not_time)
{
    // Same heights always yield same remaining — no clock input in API.
    const auto a = wallet::BuildConvergenceUiWindow(10, 20, false, false);
    const auto b = wallet::BuildConvergenceUiWindow(10, 20, false, false);
    BOOST_CHECK_EQUAL(a.remaining_blocks, b.remaining_blocks);
    BOOST_CHECK_EQUAL(a.remaining_blocks, 22); // 10+32-20
}

BOOST_AUTO_TEST_SUITE_END()
