// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_COQ_SUPPLY_FIELD_HOOK_H
#define BITCOIN_NODE_SUPPLY_MONITOR_COQ_SUPPLY_FIELD_HOOK_H

#include <consensus/amount.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;

namespace Consensus {
struct Params;
} // namespace Consensus

namespace Consensus {

/**
 * Phase-5 Coq catalog projection of `RuntimeSupplyState`.
 *
 * Field identifiers match `ReplayReorgCore/ReplayState.v` (`rs_oru_supply`,
 * `rs_res_supply`) and `ConnectBlockMint.v` (`cb_oru_subsidy`, `cb_res_mint_delta`).
 * C++ authoritative ledger uses RFC-002 recurrence (subsidy − converge); Coq replay
 * scaffold `cb_oru_mint_delta` is subsidy-only — exposed separately as audit field.
 *
 * READ-ONLY: must not be consumed by ConnectBlock or consensus validation.
 */
struct CoqSupplyFieldHook {
    // --- Chain anchor (delta / witness context) ---
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;

    // --- Cumulative ledger (ReplayState) ---
    CAmount rs_oru_supply{0}; //!< `rs_oru_supply`  ↔ RuntimeSupplyState::oru_supply_cumulative
    CAmount rs_res_supply{0}; //!< `rs_res_supply`  ↔ RuntimeSupplyState::res_mint_cumulative

    // --- Per-block deltas (ConnectBlockMint) ---
    CAmount cb_oru_subsidy{0};     //!< `cb_oru_subsidy` / `cm_oru_subsidy`
    CAmount cb_oru_burn_delta{0};  //!< C++ connect burn C(h); Coq gap — not in `cb_oru_mint_delta` yet
    CAmount cb_res_mint_delta{0};  //!< `cb_res_mint_delta` ↔ res_mint_block

    // --- RES mint cardinality ---
    /**
     * M(h) ∈ {0,1} ↔ res_mint_present_block. Kept under the historical Coq name.
     *
     * Under RFC-019 M(h) is an indicator ("this block minted"), not a mint count —
     * a block may carry many mints. The name still says "count" because it mirrors
     * the Coq symbol; reconciling the two spellings belongs to the Phase 5 pass over
     * the extraction contract, not here.
     */
    uint8_t res_mint_count_M_h{0};

    // --- Parent ledger (recurrence witnesses) ---
    CAmount rs_oru_supply_parent{0};
    CAmount rs_res_supply_parent{0};

    // --- Audit-only (no consensus / no Coq equivalence claim) ---
    std::optional<CAmount> audit_oru_subsidy_cumulative_derived;
    /** Coq `cb_oru_mint_delta` scaffold (= subsidy only); differs from C++ S_ORU step when C(h)>0. */
    CAmount audit_coq_cb_oru_mint_delta_scaffold{0};
};

/** Project mirror → Coq-named observables (pure function, no side effects). */
CoqSupplyFieldHook ProjectCoqSupplyFieldHook(const RuntimeSupplyState& mirror);

/**
 * Verify hook scalars match authoritative `CBlockIndex` caches and mirror fields.
 * For unit tests / RPC audit only.
 */
std::vector<std::string> CheckCoqHookMatchesAuthoritativeCaches(const CoqSupplyFieldHook& hook,
                                                                const RuntimeSupplyState& mirror,
                                                                const CBlockIndex& pindex);

/**
 * Resolve hook at `pindex`: stored mirror if MIRROR-2, else capture fresh (read-only).
 */
CoqSupplyFieldHook ResolveCoqSupplyFieldHook(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_COQ_SUPPLY_FIELD_HOOK_H
