// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Genesis-time canonical Consensus Kernel authority (P073).
// ConnectBlock prepares inputs, calls this Kernel, and consumes results.
// Unique formula bodies remain in issuance / RES / draw / classify / supply.
// No OCaml runtime. C ABI stays verify-only. EQUIVALENCE_PROVED = FALSE.
//
// Status is four independent facts. Do not reintroduce PRODUCTION_KERNEL_SWITCHED:
// that name was used both for "C++ kernel is production" and "extracted kernel
// is not production", which made true and false both look correct.
//
//   PRODUCTION_CONSENSUS_AUTHORITY = RigorConsensusKernel(C++)
//   FORMAL_REFINEMENT_STATUS       = PROVED   (Coq; not a C++ runtime bit)
//   EXTRACTED_KERNEL_PRODUCTION    = FALSE
//   COMPCERT_BINARY_VERIFIED_KERNEL = FALSE

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

/** Unique production consensus authority. Coq is the spec, not a runtime. */
inline constexpr const char PRODUCTION_CONSENSUS_AUTHORITY[] = "RigorConsensusKernel(C++)";
/** Extracted OCaml/C kernel is not production. Do not reuse a "switched" flag. */
inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false;
/** CompCert/VST of a production kernel object is not claimed. Deferred.
 *  The only future candidate is MakeKernelInput (pure marshalling). */
inline constexpr bool COMPCERT_BINARY_VERIFIED_KERNEL = false;
inline constexpr bool COMPCERT_VST_DEFERRED = true;
inline constexpr const char COMPCERT_VST_CANDIDATE[] = "MakeKernelInput";
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
