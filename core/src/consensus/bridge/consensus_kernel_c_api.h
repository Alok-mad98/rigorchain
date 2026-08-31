// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Stable C ABI for the Consensus Kernel. Verify-only until a later TASK
// switches ConnectBlock. Implementations call production Kernel functions;
// they do not recode issuance, draw, classify, or supply.

#ifndef BITCOIN_CONSENSUS_BRIDGE_CONSENSUS_KERNEL_C_API_H
#define BITCOIN_CONSENSUS_BRIDGE_CONSENSUS_KERNEL_C_API_H

#include <consensus/bridge/kernel_types.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
/* Production bitcoind / libbitcoin_consensus must not compile the bridge
 * translation units. The test CLI passes -DRIGOR_KERNEL_ABI_VERIFY_ONLY. */
#endif

bool rigor_compute_issuance(uint32_t height, rigor_issuance_result* out);

bool rigor_classify_script(const uint8_t* script, size_t script_len, rigor_asset_type* out);

bool rigor_coinbase_mature(uint32_t spend_height, uint32_t coin_height, uint8_t* eligible);

bool rigor_resolve_draw(
    const rigor_candidate* candidates,
    size_t count,
    const rigor_draw_context* ctx,
    rigor_draw_result* result);

bool rigor_supply_delta(const rigor_supply_input* input, rigor_supply_result* output);

#ifdef __cplusplus
}
#endif

#endif
