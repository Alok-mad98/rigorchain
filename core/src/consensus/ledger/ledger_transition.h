// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P063 verify-only UTXO Ledger Transition Kernel.
// Not compiled into production (absent from Makefile.am).
// Forbidden includes: chain.h, validation.cpp, txdb, wallet, rpc.

#ifndef BITCOIN_CONSENSUS_LEDGER_LEDGER_TRANSITION_H
#define BITCOIN_CONSENSUS_LEDGER_LEDGER_TRANSITION_H

#include <consensus/amount.h>

#include <cstdint>
#include <vector>

enum class LedgerAsset : uint8_t {
    ORU = 0,
    RES = 1,
    INVALID = 2,
};

struct LedgerOutpoint {
    uint64_t id{0};
    uint32_t n{0};

    bool operator==(const LedgerOutpoint& o) const { return id == o.id && n == o.n; }
};

struct LedgerCoin {
    LedgerOutpoint outpoint;
    CAmount value{0};
    LedgerAsset asset{LedgerAsset::INVALID};
    /** >= 0 means coinbase created at this height. -1 = not coinbase. */
    int64_t coinbase_height{-1};
};

/** Consensus Kernel accept/reject plus already-computed monetary results. */
struct ConsensusKernelResult {
    bool accept{false};
    CAmount oru_issuance{0};
    CAmount res_mint{0};
    CAmount res_schedule{0};
};

/** Transaction Kernel accept/reject. */
struct TransactionKernelResult {
    bool accept{false};
};

struct LedgerKernelInput {
    std::vector<LedgerCoin> consumed;
    std::vector<LedgerCoin> created;
    TransactionKernelResult tx;
    ConsensusKernelResult consensus;
    CAmount fee{0};
    CAmount burn{0};
    CAmount converge{0};
    int64_t height{0};
    int64_t maturity{100};
};

struct LedgerKernelOutput {
    bool accept{false};
    std::vector<LedgerOutpoint> spent_delta;
    std::vector<LedgerCoin> created_delta;
    bool value_ok{false};
};

/** C++-shaped ledger apply. Same checks as CoqLedgerApply. */
LedgerKernelOutput LedgerApply(const LedgerKernelInput& in);

#endif // BITCOIN_CONSENSUS_LEDGER_LEDGER_TRANSITION_H
