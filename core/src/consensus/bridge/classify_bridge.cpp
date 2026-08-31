// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Calls ClassifyRigorAsset only. Does not re-parse script patterns.

#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "consensus/bridge is verify-only; compile with -DRIGOR_KERNEL_ABI_VERIFY_ONLY"
#endif

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <primitives/transaction.h>

bool rigor_classify_script(const uint8_t* script, size_t script_len, rigor_asset_type* out)
{
    if (!out) return false;
    const CScript spk = RigorScriptFromBytes(script, script_len);
    switch (ClassifyRigorAsset(spk)) {
    case ASSET_ORU:
        *out = RIGOR_ASSET_ORU;
        return true;
    case ASSET_RES:
        *out = RIGOR_ASSET_RES;
        return true;
    case ASSET_NONE:
        *out = RIGOR_ASSET_NONE;
        return true;
    default:
        *out = RIGOR_ASSET_INVALID;
        return true;
    }
}
