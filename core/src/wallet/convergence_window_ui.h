// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_CONVERGENCE_WINDOW_UI_H
#define BITCOIN_WALLET_CONVERGENCE_WINDOW_UI_H

/**
 * Presentation-only W=32 convergence window helpers for GUI / acceptance tests.
 * Does NOT change consensus. Reads CONVERGENCE_DRAW_WINDOW from the lock header.
 */

#include <consensus/rigor_consensus_lock.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace wallet {

/** User-facing lifecycle for a single convergence participation (GUI state machine). */
enum class ConvergenceUiLifecycle : uint8_t {
    Submitted = 0,              //!< broadcast / waiting for inclusion
    ActiveWindow = 1,           //!< included; competing across W blocks
    FinalizationPending = 2,    //!< near end of window (block-based)
    CompletedAllocated = 3,     //!< RES allocated (node/wallet verified)
    CompletedNoAllocation = 4,  //!< window over; no RES for this entry
};

struct ConvergenceUiWindowSnapshot {
    int W{Consensus::CONVERGENCE_DRAW_WINDOW};
    int start_block{-1};         //!< inclusion height (consensus window origin for this tx)
    int current_block{-1};       //!< active tip height (block-based; never wall-clock)
    int finalization_block{-1};  //!< start_block + W
    int elapsed_in_window{0};    //!< progress numerator (0..W)
    int remaining_blocks{0};     //!< blocks until finalization_block
    ConvergenceUiLifecycle lifecycle{ConvergenceUiLifecycle::Submitted};
    bool verification_pass{false}; //!< true when outcome is known from wallet/node state
};

/**
 * Build the GUI window snapshot.
 * @param inclusion_height  Block that included the convergence tx, or -1 if not yet included.
 * @param tip_height        Active chain tip (must be block height, not time).
 * @param allocated         True if wallet/node records RES allocation for this entry.
 * @param explicit_loss     True if wallet records LOST after the window ended (ignored if premature).
 */
inline ConvergenceUiWindowSnapshot BuildConvergenceUiWindow(int inclusion_height, int tip_height,
                                                           bool allocated, bool explicit_loss)
{
    ConvergenceUiWindowSnapshot s;
    s.W = Consensus::CONVERGENCE_DRAW_WINDOW;
    s.current_block = tip_height;

    if (inclusion_height < 0) {
        s.lifecycle = ConvergenceUiLifecycle::Submitted;
        s.remaining_blocks = 0;
        s.elapsed_in_window = 0;
        return s;
    }

    s.start_block = inclusion_height;
    s.finalization_block = inclusion_height + s.W;
    s.elapsed_in_window = std::clamp(tip_height - inclusion_height, 0, s.W);
    s.remaining_blocks = std::max(0, s.finalization_block - tip_height);
    // LOST before finalization is stale metadata (old wallet marked loss at inclusion); ignore it.
    const bool window_over = tip_height >= s.finalization_block;
    const bool loss_final = explicit_loss && window_over;

    if (allocated) {
        s.lifecycle = ConvergenceUiLifecycle::CompletedAllocated;
        s.verification_pass = true;
        return s;
    }
    if (window_over || loss_final) {
        s.lifecycle = ConvergenceUiLifecycle::CompletedNoAllocation;
        s.verification_pass = true;
        s.remaining_blocks = 0;
        s.elapsed_in_window = s.W;
        return s;
    }
    // Near end of window: last 3 blocks (block-based, not wall-clock).
    if (s.remaining_blocks > 0 && s.remaining_blocks <= 3) {
        s.lifecycle = ConvergenceUiLifecycle::FinalizationPending;
        return s;
    }
    s.lifecycle = ConvergenceUiLifecycle::ActiveWindow;
    return s;
}

inline const char* ConvergenceUiLifecycleLabel(ConvergenceUiLifecycle life)
{
    switch (life) {
    case ConvergenceUiLifecycle::Submitted:
        return "Convergence started";
    case ConvergenceUiLifecycle::ActiveWindow:
        return "Convergence in progress";
    case ConvergenceUiLifecycle::FinalizationPending:
        return "Waiting for network result";
    case ConvergenceUiLifecycle::CompletedAllocated:
        return "RES result confirmed";
    case ConvergenceUiLifecycle::CompletedNoAllocation:
        return "No RES minted";
    }
    return "Unknown";
}

inline std::string ConvergenceUiLifecycleDetail(ConvergenceUiLifecycle life, bool allocated)
{
    switch (life) {
    case ConvergenceUiLifecycle::Submitted:
        return "Waiting for network confirmation.";
    case ConvergenceUiLifecycle::ActiveWindow:
        return "Your ORU participates during the next " +
               std::to_string(Consensus::CONVERGENCE_DRAW_WINDOW) +
               " blocks before the network determines the result.";
    case ConvergenceUiLifecycle::FinalizationPending:
        return "Waiting for the network to confirm the result.";
    case ConvergenceUiLifecycle::CompletedAllocated:
        // The reward is W(height) under RFC-017 and this helper has no height in scope; the
        // credited amount is shown by the transaction row itself.
        return "The scheduled RES reward was minted.";
    case ConvergenceUiLifecycle::CompletedNoAllocation:
        return "No RES minted";
    }
    (void)allocated;
    return {};
}

} // namespace wallet

#endif // BITCOIN_WALLET_CONVERGENCE_WINDOW_UI_H
