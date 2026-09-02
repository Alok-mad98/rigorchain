// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: P2P message input → network validation boundary.
// Output is only ACCEPT_TO_VALIDATION / REJECT / BAN_PEER.
//
// This module is a wire firewall. It does not:
//   - modify chainstate
//   - call GetBlockSubsidy / GetResMintReward / ResolveConvergenceDraw /
//     ClassifyRigorTxout / RigorSupplyDelta / VerifyRigorPredicate
//   - implement a script parser, signature verifier, or asset classifier
//   - emit ConsensusKernelInput
//
// Spend validity of an accepted tx MUST enter VerifyRigorPredicate
// at the transaction-admission / Transaction Kernel boundary.

#ifndef BITCOIN_NET_SECURITY_P2P_MESSAGE_BOUNDARY_H
#define BITCOIN_NET_SECURITY_P2P_MESSAGE_BOUNDARY_H

#include <net/security/p2p_security_input.h>
#include <net/security/p2p_validation_result.h>
#include <net/security/peer_misbehavior_score.h>

#include <cstddef>
#include <cstdint>

/**
 * Network-layer ceilings. Numeric values MUST match production constants:
 *   MAX_PROTOCOL_MESSAGE_LENGTH          (net.h) = 4_000_000
 *   Consensus::BLOCK_SERIALIZED_SIZE_LIMIT       = 1_000_000
 *   MAX_SCRIPT_SIZE                               = 10_000
 *   MAX_SCRIPT_ELEMENT_SIZE                       = 520
 *   RIGOR_PROTOCOL_MIN/CURRENT_VERSION            = 1 / 1
 *   DISCOURAGEMENT_THRESHOLD                      = 100
 * These aliases are not a second consensus lock.
 */
struct P2PSecurityLimits {
    static constexpr uint32_t MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
    static constexpr uint32_t BLOCK_HEADER_SIZE = 80;
    static constexpr uint32_t MAX_BLOCK_SERIALIZED_SIZE = 1000000;
    static constexpr uint32_t MAX_SCRIPT_SIZE = 10000;
    static constexpr uint32_t MAX_SCRIPT_ELEMENT_SIZE = 520;
    static constexpr int PROTOCOL_MIN_VERSION = 1;
    static constexpr int PROTOCOL_CURRENT_VERSION = 1;
    static constexpr int BITCOIN_SERIALIZATION_VERSION = 70016;
    static constexpr uint32_t MAX_MESSAGES_PER_WINDOW = 512;
    static constexpr uint32_t MAX_INVALID_BLOCKS_BEFORE_BAN = 8;
    static constexpr uint32_t MAX_INVALID_TX_BEFORE_BAN = 16;
    static constexpr uint32_t MIN_TX_SERIALIZED = 10;
};

class P2PMessageBoundary
{
public:
    /**
     * Fail-closed evaluation of one P2P message.
     * Never throws. Never mutates caller-owned chainstate (the input caches
     * are read-only). Never calls consensus formulas.
     */
    static P2PValidationResult Evaluate(const P2PSecurityInput& in);

    /** Named fuzz / audit entry points. */
    static P2PValidationResult EvaluateBlock(const uint8_t* data, size_t size,
                                             const P2PSecurityInput* ctx = nullptr);
    static P2PValidationResult EvaluateTx(const uint8_t* data, size_t size,
                                          const P2PSecurityInput* ctx = nullptr);
    static P2PValidationResult EvaluatePeer(const P2PSecurityInput& in);

    /** Network-layer fingerprint. Not a consensus txid / block hash. */
    static uint64_t WireFingerprint(const uint8_t* data, size_t size);
};

#endif // BITCOIN_NET_SECURITY_P2P_MESSAGE_BOUNDARY_H
