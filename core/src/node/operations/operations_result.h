// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// PHASE C: node operations baseline output.
// This module is engineering operations, not a consensus authority.
// Fail-closed: illegal configuration, datadir, or RPC use MUST NOT start
// or mutate chainstate.

#ifndef BITCOIN_NODE_OPERATIONS_OPERATIONS_RESULT_H
#define BITCOIN_NODE_OPERATIONS_OPERATIONS_RESULT_H

#include <cstdint>

/** Exclusive operations-layer decisions. Do not add a consensus-accept. */
enum class OperationsDecision : uint8_t {
    PASS = 0,
    FAIL_CLOSED = 1,
};

struct OperationsResult {
    OperationsDecision decision{OperationsDecision::FAIL_CLOSED};
    const char* reason{"uninitialized"};

    static OperationsResult Pass(const char* why)
    {
        OperationsResult r;
        r.decision = OperationsDecision::PASS;
        r.reason = why;
        return r;
    }

    static OperationsResult Fail(const char* why)
    {
        OperationsResult r;
        r.decision = OperationsDecision::FAIL_CLOSED;
        r.reason = why;
        return r;
    }

    bool IsPass() const { return decision == OperationsDecision::PASS; }
    bool IsFail() const { return decision == OperationsDecision::FAIL_CLOSED; }
};

#endif // BITCOIN_NODE_OPERATIONS_OPERATIONS_RESULT_H
