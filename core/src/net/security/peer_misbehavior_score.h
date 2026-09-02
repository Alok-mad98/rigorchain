// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: network-only peer score. Must not appear in ConsensusKernelInput
// and must not change a consensus decision.

#ifndef BITCOIN_NET_SECURITY_PEER_MISBEHAVIOR_SCORE_H
#define BITCOIN_NET_SECURITY_PEER_MISBEHAVIOR_SCORE_H

#include <cstdint>
#include <limits>

/**
 * Isolated peer penalty accumulator.
 *
 * Matches the existing discouragement threshold (100) as a network constant.
 * Incrementing this value never writes chainstate and never calls the Kernel.
 */
class PeerMisbehaviorScore
{
    int m_score{0};

public:
    static constexpr int BAN_THRESHOLD = 100;

    int Value() const { return m_score; }

    void Add(int delta)
    {
        if (delta <= 0) return;
        const int64_t next = static_cast<int64_t>(m_score) + static_cast<int64_t>(delta);
        if (next >= std::numeric_limits<int>::max()) {
            m_score = std::numeric_limits<int>::max();
            return;
        }
        m_score = static_cast<int>(next);
    }

    bool ShouldBan() const { return m_score >= BAN_THRESHOLD; }

    void Reset() { m_score = 0; }
};

#endif // BITCOIN_NET_SECURITY_PEER_MISBEHAVIOR_SCORE_H
