// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068 verify-only canary. Dual-executes handwritten C++ authority vs
// the stable C ABI. Not compiled into production. Does not recode formulas.
// EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
// ENABLE_CONSENSUS_KERNEL_CANARY defaults OFF.

#ifndef BITCOIN_CONSENSUS_KERNEL_CANARY_KERNEL_CANARY_H
#define BITCOIN_CONSENSUS_KERNEL_CANARY_KERNEL_CANARY_H

#if !defined(ENABLE_CONSENSUS_KERNEL_CANARY)
#error "consensus/kernel_canary is verify-only; compile with -DENABLE_CONSENSUS_KERNEL_CANARY"
#endif
#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "kernel canary needs the C ABI compile flag; not a production object"
#endif

#include <consensus/bridge/kernel_types.h>
#include <consensus/txkernel/transaction_kernel_input.h>

#include <cstddef>
#include <cstdint>

/** Canary-domain composed input. No chain / UTXO / ConnectBlock access. */
struct ConsensusKernelInput {
    uint32_t height{0};
    uint32_t spend_height{0};
    uint32_t coin_height{0};
    bool has_maturity{false};
    const uint8_t* script{nullptr};
    size_t script_len{0};
    const rigor_candidate* candidates{nullptr};
    size_t candidate_count{0};
    rigor_draw_context draw_ctx{};
    rigor_supply_input supply{};
    bool has_spend{false};
    TransactionKernelInput tx{};
};

struct KernelResult {
    bool accept{false};
    uint64_t oru_subsidy{0};
    uint64_t res_schedule{0};
    uint64_t res_mint_expectation{0};
    uint32_t epoch{0};
    uint8_t draw_has_winner{0};
    rigor_u256 draw_winner{};
    rigor_asset_type classification{RIGOR_ASSET_INVALID};
    int64_t ledger_delta{0};
    uint64_t coinbase_expectation{0};
    bool spend_ok{true};
    bool supply_ok{false};
    bool mature{true};
};

class KernelCanaryExecutor
{
public:
    bool RunCpp(const ConsensusKernelInput& in, KernelResult& out) const;
    bool RunAbi(const ConsensusKernelInput& in, KernelResult& out) const;

    /** Dual execution. FAIL HARD on mismatch. Never auto-correct. */
    bool Compare(const ConsensusKernelInput& in, KernelResult& out, const char** field) const;

    /** Deterministic same-input battery. Returns discrepancy count (0 = PASS). */
    int RunDeterministicBattery(int iterations, const char** first_field) const;
};

#endif
