// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_GBT_STARTUP_POLICY_H
#define BITCOIN_NODE_GBT_STARTUP_POLICY_H

#include <uint256.h>

#include <string>

namespace node {

/** Published RIGOR mainnet genesis block hash. RPC policy pin, not a consensus rule. */
inline const uint256& MainnetGenesisIdentity()
{
    static const uint256 hash = uint256S("0xe2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468");
    return hash;
}

enum class GbtStartupStatus {
    kAllow,
    kWrongNetwork,
    kGenesisMismatch,
    kNoEligiblePeer,
    kInitialBlockDownload,
};

/** Facts observed by getblocktemplate. The evaluator does not touch consensus. */
struct GbtStartupFacts {
    bool is_test_chain{false};
    //! Explicit -solomining. Default false. Skips only the peer and IBD refusals.
    bool solo_mining{false};
    std::string network_id;
    uint256 params_genesis;
    uint256 genesis_block_hash;
    bool have_active_genesis{false};
    uint256 active_genesis;
    //! Handshake-complete peers that can relay blocks. Not feelers or addr-fetch.
    int handshake_peers{0};
    bool initial_block_download{false};
};

inline constexpr char GBT_WRONG_NETWORK[] =
    "getblocktemplate production policy requires chain main";
inline constexpr char GBT_GENESIS_MISMATCH[] =
    "getblocktemplate genesis identity does not match the RIGOR mainnet genesis";

/**
 * Production template gate.
 *
 * Test chains are unchanged: callers keep the historical regtest/devtest path.
 * On mainnet, network identity and genesis identity are checked even when
 * -solomining is set. That flag skips only the eligible-peer and IBD refusals.
 */
inline GbtStartupStatus EvaluateGbtStartup(const GbtStartupFacts& facts)
{
    if (facts.is_test_chain) {
        return GbtStartupStatus::kAllow;
    }
    if (facts.network_id != "main") {
        return GbtStartupStatus::kWrongNetwork;
    }
    const uint256& expected = MainnetGenesisIdentity();
    if (facts.params_genesis != expected || facts.genesis_block_hash != expected) {
        return GbtStartupStatus::kGenesisMismatch;
    }
    if (!facts.have_active_genesis) {
        return GbtStartupStatus::kInitialBlockDownload;
    }
    if (facts.active_genesis != expected) {
        return GbtStartupStatus::kGenesisMismatch;
    }
    if (facts.solo_mining) {
        return GbtStartupStatus::kAllow;
    }
    if (facts.handshake_peers < 1) {
        return GbtStartupStatus::kNoEligiblePeer;
    }
    if (facts.initial_block_download) {
        return GbtStartupStatus::kInitialBlockDownload;
    }
    return GbtStartupStatus::kAllow;
}

} // namespace node

#endif // BITCOIN_NODE_GBT_STARTUP_POLICY_H
