// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_EXPORT_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_EXPORT_H

#include <node/supply_monitor/coq_supply_field_hook.h>
#include <node/supply_monitor/runtime_supply_state.h>
#include <uint256.h>

#include <cstdint>
#include <string>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class UniValue;

namespace Consensus {
struct Params;
} // namespace Consensus

namespace Consensus {

/** How the mirror was obtained for this RPC/export response (MIRROR-2 / MIRROR-3). */
enum class RuntimeSupplyMirrorSource {
    STORED_MIRROR,   //!< Post-ConnectBlock copy on CBlockIndex
    CAPTURED_FRESH,  //!< Warm caches + CaptureRuntimeSupplyState
};

/**
 * TASK-FVR-D read-only bundle: mirror + authoritative pindex caches + Coq hook.
 * Must not be used by validation or state_hash paths.
 */
struct RuntimeSupplyExport {
    RuntimeSupplyState mirror;
    CoqSupplyFieldHook coq_hook;
    RuntimeSupplyMirrorSource mirror_source{RuntimeSupplyMirrorSource::CAPTURED_FRESH};

    CAmount authoritative_oru_supply_cumulative{-1};
    CAmount authoritative_oru_converge_burn_block{0};
    CAmount authoritative_res_mint_cumulative{-1};
    CAmount authoritative_res_mint_block{0};
};

/**
 * Resolve export at `pindex` (stored mirror if valid, else capture).
 * Warms supply ledger caches only; does not alter consensus.
 */
RuntimeSupplyExport BuildRuntimeSupplyExport(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus);

/** Standard JSON for `getruntimesupply` (all ledger + mirror + coq_fields + phase5 cells). */
UniValue RuntimeSupplyExportToJson(const RuntimeSupplyExport& exp,
                                   const CBlockIndex& pindex,
                                   const Params& consensus);

/**
 * Phase-5 audit grid: 15 supply observables × 2 sources = 30 cells.
 * Sources: `runtime_mirror` (observational) and `authoritative_index` (pindex caches).
 */
UniValue RuntimeSupplyPhase5CellsToJson(const RuntimeSupplyExport& exp);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_EXPORT_H
