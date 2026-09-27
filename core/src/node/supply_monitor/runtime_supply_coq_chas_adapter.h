// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_COQ_CHAS_ADAPTER_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_COQ_CHAS_ADAPTER_H

#include <consensus/amount.h>
#include <node/supply_monitor/runtime_supply_witness_coq_adapter.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/** Phase-5 CanonicalHashAlignmentStatus (FVR-H). */
enum class CanonicalHashAlignmentStatus {
    Aligned,                //!< CHAS_Aligned — not assigned in FVR-H
    Partial,                //!< CHAS_Partial — runtime scalar witness exported
    Missing,                //!< CHAS_Missing — no witness at height
    FundamentallyBlocked,   //!< CHAS_FundamentallyBlocked — invariant / chain break
};

/** Stable string id for RPC / JSON export. */
const char* ChasAlignmentStatusToString(CanonicalHashAlignmentStatus status);

/**
 * Per-field CHAS alignment vector for a supply cumulative scalar.
 *
 * Maps Coq catalog `rs_oru_supply` / `rs_res_supply` → `chas_oru_state` /
 * `chas_res_state` without asserting hash equality to Coq abstract folds.
 */
struct RuntimeSupplyChasFieldAlignment {
    std::string chas_field;         //!< e.g. chas_oru_state
    std::string coq_catalog_field;  //!< e.g. rs_oru_supply
    CanonicalHashAlignmentStatus status{CanonicalHashAlignmentStatus::Missing};
    CAmount runtime_value{0};
    bool runtime_witness_present{false};
    bool maps_to_canonical_hash{true};
    bool theoretical_alignment_potential{true};
    std::vector<std::string> audit_notes;
};

/** Full per-block CHAS alignment report from a witness catalog row. */
struct RuntimeSupplyChasAlignmentReport {
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;

    bool mirror_present{false};
    std::string mirror_source{"witness_only"};

    bool invariants_passed{true};
    uint32_t checked_invariants{RUNTIME_SUPPLY_INVARIANT_COUNT};
    std::vector<std::string> violations;

    RuntimeSupplyWitnessCoqDelta delta;
    std::vector<std::string> delta_audit_violations;

    RuntimeSupplyChasFieldAlignment chas_oru_state;
    RuntimeSupplyChasFieldAlignment chas_res_state;

    bool equivalence_claimed{false};
    std::string coq_equiv_runtime{"NOT_ESTABLISHED"};
};

/** Build CHAS alignment vectors from a resolved catalog row. */
RuntimeSupplyChasAlignmentReport BuildChasAlignmentFromCatalogRow(
    const RuntimeSupplyWitnessCoqCatalogRow& row);

/**
 * Resolve CHAS report at `pindex` (witness chain + stored mirror only).
 */
std::optional<RuntimeSupplyChasAlignmentReport> ResolveChasAlignmentAt(const CBlockIndex* pindex);

/** JSON for `getphase5runtimealignment` RPC. */
UniValue ChasAlignmentReportToJson(const RuntimeSupplyChasAlignmentReport& report);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_COQ_CHAS_ADAPTER_H
