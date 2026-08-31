// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Prepared Kernel inputs only. No chain / UTXO / ConnectBlock access.

#ifndef BITCOIN_CONSENSUS_SHADOW_KERNEL_INPUT_H
#define BITCOIN_CONSENSUS_SHADOW_KERNEL_INPUT_H

#ifndef RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#error "consensus/shadow is verify-only; compile with -DRIGOR_KERNEL_SHADOW_VERIFY_ONLY"
#endif

#include <consensus/bridge/kernel_types.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ConsensusKernelSurface {
    CK_SURFACE_ISSUANCE = 0,
    CK_SURFACE_CLASSIFY = 1,
    CK_SURFACE_DRAW = 2,
    CK_SURFACE_SUPPLY = 3
} ConsensusKernelSurface;

typedef struct ConsensusKernelInput {
    ConsensusKernelSurface surface;
    uint32_t height;
    const uint8_t* script;
    size_t script_len;
    const rigor_candidate* candidates;
    size_t candidate_count;
    rigor_draw_context draw_ctx;
    rigor_supply_input supply;
} ConsensusKernelInput;

#ifdef __cplusplus
}

#include <string>

struct ConsensusKernelShadowOutput {
    rigor_issuance_result issuance{};
    rigor_asset_type asset{RIGOR_ASSET_INVALID};
    rigor_draw_result draw{};
    rigor_supply_result supply{};
    bool supply_ok{false};
    int64_t ledger_oru_delta{0};
};

struct ConsensusKernelShadowDiscrepancy {
    bool found{false};
    std::string surface;
    std::string cpp_json;
    std::string ext_json;
};

#endif

#endif
