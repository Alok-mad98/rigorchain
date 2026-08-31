// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Verify-only. Calls production Kernel and C ABI. Does not recode formulas.
// Does not replace ConnectBlock.

#ifndef BITCOIN_CONSENSUS_SHADOW_SHADOW_EXECUTOR_H
#define BITCOIN_CONSENSUS_SHADOW_SHADOW_EXECUTOR_H

#ifndef RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#error "consensus/shadow is verify-only; compile with -DRIGOR_KERNEL_SHADOW_VERIFY_ONLY"
#endif

#include <consensus/shadow/kernel_input.h>

class ConsensusKernelShadowExecutor
{
public:
    bool RunCpp(const ConsensusKernelInput& in, ConsensusKernelShadowOutput& out) const;
    bool RunAbi(const ConsensusKernelInput& in, ConsensusKernelShadowOutput& out) const;

    /** Compare C++ Kernel vs C ABI. On mismatch: do not repair; report STOP. */
    ConsensusKernelShadowDiscrepancy Compare(const ConsensusKernelInput& in) const;

    /** Deterministic same-input battery. Returns discrepancy count (0 = PASS). */
    int RunDeterministicBattery(int iterations, ConsensusKernelShadowDiscrepancy* first) const;
};

#endif
