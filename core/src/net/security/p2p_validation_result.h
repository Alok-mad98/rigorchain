// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: P2P security-boundary output. The network layer may emit only these
// three decisions. It must not mutate chainstate or enter ConsensusKernel.

#ifndef BITCOIN_NET_SECURITY_P2P_VALIDATION_RESULT_H
#define BITCOIN_NET_SECURITY_P2P_VALIDATION_RESULT_H

#include <cstdint>

/** Exclusive P2P-layer decisions. Do not add a fourth. */
enum class P2PValidationDecision : uint8_t {
    ACCEPT_TO_VALIDATION = 0,
    REJECT = 1,
    BAN_PEER = 2,
};

/** Where ACCEPT_TO_VALIDATION is allowed to go next. Never ConsensusKernel. */
enum class P2PAdmissionTarget : uint8_t {
    NONE = 0,
    BLOCK_ADMISSION = 1, // CheckBlock / ProcessNewBlock — not Kernel
    TX_ADMISSION = 2,    // CheckTransaction / ATMP — spend = VerifyRigorPredicate
    HANDSHAKE = 3,
    OTHER = 4,
};

struct P2PValidationResult {
    P2PValidationDecision decision{P2PValidationDecision::REJECT};
    P2PAdmissionTarget admission{P2PAdmissionTarget::NONE};
    const char* reason{"uninitialized"};
    /** Structural tx that looks spendable must still enter VerifyRigorPredicate. */
    bool spend_must_enter_verify_rigor_predicate{false};
    /** True iff this result is a network-only peer penalty and must not affect consensus. */
    bool peer_score_isolated{true};

    static P2PValidationResult Accept(P2PAdmissionTarget target, const char* why,
                                      bool spend_predicate = false)
    {
        P2PValidationResult r;
        r.decision = P2PValidationDecision::ACCEPT_TO_VALIDATION;
        r.admission = target;
        r.reason = why;
        r.spend_must_enter_verify_rigor_predicate = spend_predicate;
        r.peer_score_isolated = true;
        return r;
    }

    static P2PValidationResult Reject(const char* why)
    {
        P2PValidationResult r;
        r.decision = P2PValidationDecision::REJECT;
        r.admission = P2PAdmissionTarget::NONE;
        r.reason = why;
        r.spend_must_enter_verify_rigor_predicate = false;
        r.peer_score_isolated = true;
        return r;
    }

    static P2PValidationResult Ban(const char* why)
    {
        P2PValidationResult r;
        r.decision = P2PValidationDecision::BAN_PEER;
        r.admission = P2PAdmissionTarget::NONE;
        r.reason = why;
        r.spend_must_enter_verify_rigor_predicate = false;
        r.peer_score_isolated = true;
        return r;
    }

    bool IsAccept() const { return decision == P2PValidationDecision::ACCEPT_TO_VALIDATION; }
    bool IsReject() const { return decision == P2PValidationDecision::REJECT; }
    bool IsBan() const { return decision == P2PValidationDecision::BAN_PEER; }
};

#endif // BITCOIN_NET_SECURITY_P2P_VALIDATION_RESULT_H
