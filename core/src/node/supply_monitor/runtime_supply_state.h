// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_STATE_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_STATE_H

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
 * Read-only runtime mirror of ORU/RES mint ledgers .
 *
 * STRICTLY OBSERVATIONAL: must not be read by ConnectBlock validation predicates
 * or influence block acceptance.
 *
 * FENCE-W1: ObservationalMetrics path-fold caches (`GetChainOruTotal`,
 * `GetChainResMintTotal`, plus per-block `m_block_converge_sum` /
 * `m_block_res_mint_sum`) are observational ledger scalars.
 * They are **not** spendable balances (spendable money is the UTXO set). The RES hard
 * cap is RFC-020 `RES_MAX_SUPPLY` (`2²²` RES), enforced per mint by the pole predicate
 * `R · (2²⁵+h)² ≤ B << 40` (`CheckResMintArithmetic`). Height `< 0` cannot mint.
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
    CAmount res_mint_block{0};          //!< RES minted at this height ↔ m_block_res_mint_sum
    CAmount res_mint_parent{0};         //!< S_RES(h-1)
    /**
     * M(h) ∈ {0,1} — an INDICATOR, not a count: 1 iff this block minted any RES.
     *
     * Under the retired lottery a block had at most one winner, so "did it mint" and
     * "how many mints" were the same number. RFC-020 lets any number of transactions
     * mint in the same block at destroy_cost(h), so the two diverge. The amount
     * is carried in full by `res_mint_block`; this field only records presence, which
     * is what RES-L6 needs to rule out RES appearing with no minting transaction.
     */
    uint8_t res_mint_present_block{0};

    // --- Constitutional constants (copied for audit export) ---
    CAmount res_max_supply{0};

    // --- UTXO aggregates (diagnostic; same predicates as STEP-0101 trailer) ---
    CAmount utxo_res_balance{0};
    CAmount utxo_non_res_balance{0};

    // --- Optional audit ---
    std::optional<CAmount> oru_subsidy_cumulative_derived;
};

/** Sum ORU block subsidies R(i) for i ∈ [0, height]. RFC-020 closed form: height * 128 ORU. */
CAmount ComputeDerivedOruSubsidyCumulative(int height, const Params& consensus);

/**
 * Scan a coins view for RES / non-RES balance totals (diagnostic / RPC only).
 * MUST NOT be called from ConnectBlock: CCoinsViewCache::Cursor() is O(UTXO).
 */
void AggregateUtxoSupplyBalances(const CCoinsViewCache& view,
                                 CAmount& utxo_res_balance,
                                 CAmount& utxo_non_res_balance);

/**
 * Ensure memory-only ledger caches on `pindex` and ancestors are populated
 * (`FoldChainResMintTotal` / `FoldChainOruTotal`). Invalid folds leave the
 * cache unset; they do not write 0. Does not touch mirrors.
 */
void WarmSupplyLedgerCaches(CBlockIndex* pindex, const Params& consensus);

/**
 * Build a supply mirror from authoritative `pindex` fields and optional UTXO scan.
 *
 * @param res_mint_count_block  Number of minting transactions seen in the block during
 *                              ConnectBlock. Stored as the indicator M(h); see the field.
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
 * Verify formal invariants plus diagnostic UTXO-2 (debug / export / RPC).
 * Delegates to `VerifyRuntimeSupplyInvariants`; appends `UTXO-2` when applicable.
 * @return Stable invariant ids (e.g. `"ORU-L3"`, `"RES-L5"`); empty if all pass.
 */
std::vector<std::string> CheckRuntimeSupplyInvariants(const RuntimeSupplyState& state,
                                                      const Params& consensus);

} // namespace Consensus

// Phase-5 Coq catalog projection: see node/supply_monitor/coq_supply_field_hook.h .

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_STATE_H
