// Copyright (c) 2011-2022 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INTERFACES_CONVERGENCE_BLOCK_RESULT_H
#define BITCOIN_INTERFACES_CONVERGENCE_BLOCK_RESULT_H

#include <uint256.h>

#include <cstdint>
#include <string>

namespace interfaces {

//! Convergence draw / ConvergenceRoot set summary for one block (GUI; mirrors getconvergenceresult core fields).
struct ConvergenceBlockResult {
    //! True when the block was resolved and fields below are valid for that height.
    bool ok{false};
    int height{-1};
    //! N = |F| from Consensus::ListConvergenceRootCommitmentTxids (same as getconvergenceresult).
    int64_t convergence_root_set_size{0};
    //! ConvergenceDrawCandidates tie-break winner txid; null when none (genesis, no undo, or no unique winner).
    uint256 winner_txid{};
    //! True if any of the caller-supplied wallet convergence txids is in F.
    bool is_mine{false};
    //! Non-coinbase transactions in the block (same count basis as getconvergenceresult candidates array).
    int candidates_count{0};
    //! Set when ok is false: out-of-range height, missing block data, read failure, etc.
    std::string error_message;
};

} // namespace interfaces

#endif // BITCOIN_INTERFACES_CONVERGENCE_BLOCK_RESULT_H
