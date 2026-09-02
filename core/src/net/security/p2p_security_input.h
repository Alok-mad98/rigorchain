// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: P2P message input to the network validation boundary.
// This structure is not ConsensusKernelInput and must never be one.

#ifndef BITCOIN_NET_SECURITY_P2P_SECURITY_INPUT_H
#define BITCOIN_NET_SECURITY_P2P_SECURITY_INPUT_H

#include <cstdint>
#include <set>
#include <string>
#include <vector>

/**
 * Wire + peer context for P2PMessageBoundary::Evaluate.
 *
 * Forbidden contents (enforced by lint, not by this type):
 *   chainstate, UTXO, subsidy, RES reward, draw set, supply delta,
 *   script classifier, signature, ConsensusKernelInput.
 */
struct P2PSecurityInput {
    std::string command;
    std::vector<uint8_t> payload;
    /** Claimed framing size. 0 means payload.size(). */
    uint64_t claimed_size{0};

    uint64_t peer_id{0};
    uint32_t messages_in_window{0};
    uint32_t invalid_blocks_from_peer{0};
    uint32_t invalid_tx_from_peer{0};
    bool handshake_complete{true};

    /** Network-layer replay / duplicate caches. Not consensus uniqueness. */
    std::set<uint64_t> seen_block_keys;
    std::set<uint64_t> seen_tx_keys;
};

#endif // BITCOIN_NET_SECURITY_P2P_SECURITY_INPUT_H
