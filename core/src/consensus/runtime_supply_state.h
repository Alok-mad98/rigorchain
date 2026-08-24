// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_STATE_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_STATE_H

#include <consensus/amount.h>
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
 * Read-only runtime mirror of ORU/RES mint ledgers (TASK-FVR-B).
 *
 * STRICTLY OBSERVATIONAL: must not be read by ConnectBlock validation predicates
 * or influence block acceptance.
 *
 * FENCE-W1: ObservationalMetrics path-fold caches (`GetChainOruTotal`,
 * `GetChainResMintTotal`, plus per-block `m_block_converge_sum` /
 * `m_block_res_mint_sum`) are observational ledger scalars.
 * They are **not** spendable balances (spendable money is the UTXO set). The RES hard
 * cap is RFC-017 `ResScheduleSum()` plus mint authorization (winner, unique, `W(h)`).
 * ConnectBlock does not consult these scalars. Do not feed them into T, f_i, or script evaluation.
 *
 * Field names align with Phase-5 Coq replay scalars (`rs_oru_supply`,
 * `rs_res_supply`) and RFC-002 / RFC-015 ledger notation.
 */
struct RuntimeSupplyState {
    // --- Identity ---
    int32_t height{-1};
    uint256 block_hash;
    uint256 parent_hash;

    // --- ORU ledger (RFC-002) ---
    CAmount oru_supply_cumulative{0};   //!< S_ORU(h)  ↔ ObservationalMetrics::GetChainOruTotal
    CAmount oru_subsidy_block{0};       //!< R(h)      ↔ GetBlockSubsidy
    CAmount oru_converge_burn_block{0}; //!< C(h)      ↔ m_block_converge_sum
    CAmount oru_supply_parent{0};       //!< S_ORU(h-1)

    // --- RES mint ledger (RFC-015) ---
    CAmount res_mint_cumulative{0};     //!< S_RES(h)  ↔ ObservationalMetrics::GetChainResMintTotal
    CAmount res_mint_block{0};          //!< M(h)·RES_REWARD_AMOUNT ↔ m_block_res_mint_sum
    CAmount res_mint_parent{0};         //!< S_RES(h-1)
    uint8_t res_mint_count_block{0};    //!< M(h) ∈ {0,1}

    // --- Constitutional constants (copied for audit export) ---
    CAmount res_max_supply{0};
    CAmount res_reward_amount{0};   //!< W(height) per RFC-017, not a fixed constant

    // --- UTXO aggregates (diagnostic; same predicates as STEP-0101 trailer) ---
    CAmount utxo_res_balance{0};
    CAmount utxo_non_res_balance{0};

    // --- Optional audit ---
    std::optional<CAmount> oru_subsidy_cumulative_derived;
};

/** Sum ORU block subsidies R(i) for i ∈ [0, height] (audit-only; O(height)). */
CAmount ComputeDerivedOruSubsidyCumulative(int height, const Params& consensus);

/**
 * Scan the active coins view for RES / non-RES balance totals (diagnostic only).
 */
void AggregateUtxoSupplyBalances(const CCoinsViewCache& view,
                                 CAmount& utxo_res_balance,
                                 CAmount& utxo_non_res_balance);

/**
 * Ensure memory-only ledger caches on `pindex` and ancestors are populated
 * (same algorithm as validation.cpp GetChain* helpers). Does not touch mirrors.
 */
void WarmSupplyLedgerCaches(CBlockIndex* pindex, const Params& consensus);

/**
 * Build a supply mirror from authoritative `pindex` fields and optional UTXO scan.
 *
 * @param res_mint_count_block  M(h) ∈ {0,1} observed during ConnectBlock.
 * @param oru_supply_parent_opt  If set, S_ORU(h-1) from ConnectBlock (required at connect).
 * @param res_mint_parent_opt    If set, S_RES(h-1) from ConnectBlock (required at connect).
 *                               When omitted (RPC refresh), parents are read from warmed caches.
 */
RuntimeSupplyState CaptureRuntimeSupplyState(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus,
                                             uint8_t res_mint_count_block,
                                             std::optional<CAmount> oru_supply_parent_opt = std::nullopt,
                                             std::optional<CAmount> res_mint_parent_opt = std::nullopt);

/** Store mirror on `pindex` (memory-only; not serialized; not in state_hash). */
void StoreRuntimeSupplyMirror(CBlockIndex* pindex, RuntimeSupplyState state); // non-const: connect path only

/** Drop mirror on disconnect / reorg (paired with cache invalidation). */
void InvalidateRuntimeSupplyMirror(const CBlockIndex* pindex);

/** @return Stored mirror if present. */
const std::optional<RuntimeSupplyState>& GetRuntimeSupplyMirror(const CBlockIndex& pindex);

/**
 * Verify TASK-FVR-E formal invariants plus diagnostic UTXO-2 (debug / export / RPC).
 * Delegates to `VerifyRuntimeSupplyInvariants`; appends `UTXO-2` when applicable.
 * @return Stable invariant ids (e.g. `"ORU-L3"`, `"RES-L5"`); empty if all pass.
 */
std::vector<std::string> CheckRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                      const Params& consensus);

} // namespace Consensus

// Phase-5 Coq catalog projection: see coq_supply_field_hook.h (TASK-FVR-C).

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_STATE_H
