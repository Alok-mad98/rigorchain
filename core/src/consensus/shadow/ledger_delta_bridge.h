// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Layer map only. Does not recode subsidy or burn.

#ifndef BITCOIN_CONSENSUS_SHADOW_LEDGER_DELTA_BRIDGE_H
#define BITCOIN_CONSENSUS_SHADOW_LEDGER_DELTA_BRIDGE_H

#ifndef RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#error "consensus/shadow is verify-only; compile with -DRIGOR_KERNEL_SHADOW_VERIFY_ONLY"
#endif

#include <cstdint>

/** C++ RigorSupplyDelta.oru_circulating_delta is the RFC-002 ledger Δ.
 *  That field maps to Coq ledger_oru_delta (subsidy − converge).
 *  It does not map to Coq apply_delta circulating (also subtracts burn).
 */
inline int64_t LedgerDeltaBridge(int64_t cpp_oru_circulating_delta)
{
    return cpp_oru_circulating_delta;
}

#endif
