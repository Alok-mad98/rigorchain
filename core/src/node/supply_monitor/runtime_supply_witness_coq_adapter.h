// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_COQ_ADAPTER_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_COQ_ADAPTER_H

#include <node/supply_monitor/coq_supply_field_hook.h>
#include <node/supply_monitor/runtime_supply_export.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/**
 * Parent→child cumulative deltas for Phase-5 Coq replay rows.
 * Computed from in-memory witness chain only.
 */
struct RuntimeSupplyWitnessCoqDelta {
    bool parent_witness_present{false};
    std::optional<int32_t> parent_height;
    std::optional<uint256> parent_block_hash;
    CAmount rs_oru_supply_delta{0};
    CAmount rs_res_supply_delta{0};
    bool witness_chain_step_valid{false};
};

/**
 * Phase-5 Coq catalog row derived from a historical `RuntimeSupplyWitness`.
 *
 * Maps witness cumulatives → `rs_oru_supply` / `rs_res_supply`; attaches invariant
 * audit, optional mirror hook, and parent deltas. Observational only.
 */
struct RuntimeSupplyWitnessCoqCatalogRow {
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;

    CAmount rs_oru_supply{0};
    CAmount rs_res_supply{0};

    bool invariants_passed{true};
    uint32_t checked_invariants{RUNTIME_SUPPLY_INVARIANT_COUNT};
    std::vector<std::string> violations;

    CoqSupplyFieldHook coq_hook;
    bool mirror_present{false};
    RuntimeSupplyMirrorSource mirror_source{RuntimeSupplyMirrorSource::CAPTURED_FRESH};

    RuntimeSupplyWitnessCoqDelta delta;

    /** Always false until a future phase explicitly establishes equivalence. */
    bool equivalence_claimed{false};
};

/** Build catalog row from witness + optional stored mirror + optional parent witness. */
RuntimeSupplyWitnessCoqCatalogRow BuildWitnessCoqCatalogRow(
    const RuntimeSupplyWitness& witness,
    const std::optional<RuntimeSupplyState>& mirror,
    bool mirror_present,
    RuntimeSupplyMirrorSource mirror_source,
    const std::optional<RuntimeSupplyWitness>& parent_witness);

/**
 * Resolve row at `pindex` using in-memory witness chain + stored mirror only.
 * Does not warm caches or capture fresh mirrors.
 */
std::optional<RuntimeSupplyWitnessCoqCatalogRow> ResolveWitnessCoqCatalogRowAt(const CBlockIndex* pindex);

/**
 * Verify parent cumulative deltas match per-block hook deltas when mirror present.
 * @return Empty if consistent; else stable audit ids.
 */
std::vector<std::string> CheckWitnessCoqDeltaMatchesHook(const RuntimeSupplyWitnessCoqCatalogRow& row);

/** JSON for `getruntimesupplywitnesscatalog` RPC. */
UniValue WitnessCoqCatalogRowToJson(const RuntimeSupplyWitnessCoqCatalogRow& row);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_COQ_ADAPTER_H
