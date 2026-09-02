// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P072 canary health latch. ENABLE_CONSENSUS_KERNEL_CANARY is the
// verify-only dual-path guard. Does not recode authorities.
// EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)

#ifndef BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_HEALTH_H
#define BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_HEALTH_H

#include <atomic>
#include <cstdint>

enum class ConsensusKernelCanaryHealthState : uint8_t {
    ACTIVE = 0,
    PAUSED = 1,
    DISABLED = 2,
};

class ConsensusKernelCanaryHealth
{
    std::atomic<uint8_t> m_state{static_cast<uint8_t>(ConsensusKernelCanaryHealthState::PAUSED)};

public:
    static ConsensusKernelCanaryHealth& Instance()
    {
        static ConsensusKernelCanaryHealth g;
        return g;
    }

    ConsensusKernelCanaryHealthState State() const
    {
        return static_cast<ConsensusKernelCanaryHealthState>(m_state.load());
    }

    const char* Status() const
    {
        switch (State()) {
        case ConsensusKernelCanaryHealthState::ACTIVE: return "ACTIVE";
        case ConsensusKernelCanaryHealthState::PAUSED: return "PAUSED";
        case ConsensusKernelCanaryHealthState::DISABLED: return "DISABLED";
        }
        return "PAUSED";
    }

    void SetActive() { m_state.store(static_cast<uint8_t>(ConsensusKernelCanaryHealthState::ACTIVE)); }
    void SetPaused() { m_state.store(static_cast<uint8_t>(ConsensusKernelCanaryHealthState::PAUSED)); }
    void SetDisabled() { m_state.store(static_cast<uint8_t>(ConsensusKernelCanaryHealthState::DISABLED)); }

    bool AllowsCompare() const { return State() == ConsensusKernelCanaryHealthState::ACTIVE; }

    void Reset() { SetPaused(); }
};

#endif
