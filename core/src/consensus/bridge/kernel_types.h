// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// POD types for the Consensus Kernel C ABI. No C++ class, container, or
// chain/UTXO object.

#ifndef BITCOIN_CONSENSUS_BRIDGE_KERNEL_TYPES_H
#define BITCOIN_CONSENSUS_BRIDGE_KERNEL_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 32 raw bytes; memcpy-compatible with uint256::begin() (not a C++ class). */
typedef struct rigor_u256 {
    uint8_t bytes[32];
} rigor_u256;

typedef struct rigor_issuance_result {
    uint64_t oru_subsidy;
    /** RFC-020 display destroy_cost(h): ORU atoms to mint one whole RES at this height. */
    uint64_t res_destroy_cost;
    uint32_t epoch;
} rigor_issuance_result;

typedef enum rigor_asset_type {
    RIGOR_ASSET_NONE = 0,
    RIGOR_ASSET_ORU = 1,
    RIGOR_ASSET_RES = 2,
    /* Fail-closed. Production ClassifyRigorAsset returns ASSET_INVALID (4). */
    RIGOR_ASSET_INVALID = 3
} rigor_asset_type;

/**
 * height is required by RigorSupplyDelta (kernel computes subsidy).
 * subsidy is recorded for oracles; the adapter does not use it as a formula.
 */
typedef struct rigor_supply_input {
    uint32_t height;
    uint64_t subsidy;
    uint64_t converge;
    uint64_t burn;
    uint64_t mint;
    uint64_t fees_oru;
    uint64_t oru_coinbase;
    uint8_t has_winner;
    uint32_t card;
} rigor_supply_input;

typedef struct rigor_supply_result {
    int64_t delta;
} rigor_supply_result;

#ifdef __cplusplus
}
#endif

#endif
