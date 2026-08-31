// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Genesis-time canonical Consensus Kernel authority (P073).
// ConnectBlock prepares inputs, calls this Kernel, and consumes results.
// Unique formula bodies remain in issuance / RES / draw / classify / supply.
// No OCaml runtime. C ABI stays verify-only. EQUIVALENCE_PROVED = FALSE.

#ifndef BITCOIN_CONSENSUS_KERNEL_GENESIS_AUTHORITY_H
#define BITCOIN_CONSENSUS_KERNEL_GENESIS_AUTHORITY_H

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>

inline constexpr bool PRODUCTION_KERNEL_SWITCHED = true;
inline constexpr bool GENESIS_SINGLE_PRODUCTION_AUTHORITY = true;
inline constexpr bool GENESIS_EQUIVALENCE_PROVED = false;

struct RigorConsensusKernelConnectInput {
    int height{0};
    const CTransaction* coinbase{nullptr};
    ConvergenceDrawSet parent_draw;
    ConvergenceDrawOutcome parent_draw_outcome;
    RigorSupplyDeltaInput supply;
    const Consensus::Params* params{nullptr};
};

struct RigorConsensusKernelConnectResult {
    bool accept{false};
    CAmount res_mint{0};
    RigorSupplyDeltaResult supply;
};

class RigorConsensusKernel
{
public:
    /** Unique production draw entry. Body remains ResolveConvergenceDraw. */
    static ConvergenceDrawOutcome ResolveDraw(const ConvergenceDrawSet& F, const uint256& T);

    /** Unique production issuance pack. Body remains ComputeRigorIssuance. */
    static RigorIssuance Issuance(int height, const Consensus::Params& params);

    /** Unique production classifier. Body remains ClassifyRigorAsset. */
    static AssetType Classify(const CScript& script);

    /** Coinbase maturity predicate used by the Kernel (same inequality as tx_verify). */
    static bool CoinbaseMature(int spend_height, int coin_height, int64_t maturity);

    /**
     * Connect-time Kernel: RES mint gate then supply delta.
     * Does not mutate UTXO / chainstate. ConnectBlock applies state after accept.
     */
    static bool EvaluateConnect(const RigorConsensusKernelConnectInput& in,
        RigorConsensusKernelConnectResult& out, BlockValidationState& state);
};

#endif
