// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RIGOR_CONSENSUS_LOCK_H
#define BITCOIN_CONSENSUS_RIGOR_CONSENSUS_LOCK_H

#include <consensus/amount.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * Rigor consensus lock — single source of truth for cross-module numeric locks.
 *
 * - ORU scale remains `COIN` from amount.h (Bitcoin-compatible CAmount).
 * - Do not duplicate these values in other headers; add `static_assert` bridges if needed.
 * - `RIGOR_CONSENSUS_LOCK_VERSION` bumps only on intentional hard-forking lock changes.
 */
namespace Consensus {

/** Hard-forking identifier for this lock bundle (not P2P PROTOCOL_VERSION). */
inline constexpr int RIGOR_CONSENSUS_LOCK_VERSION = 2;
static_assert(RIGOR_CONSENSUS_LOCK_VERSION == 2, "Rigor consensus lock version mismatch");

// ---------------------------------------------------------------------------
// On-chain block space (1 MiB hard cap; capacity is entirely on-chain)
// Aliased as MAX_BLOCK_* in consensus/consensus.h — do not duplicate elsewhere.
// ---------------------------------------------------------------------------

/** Absolute serialized block ceiling (bytes). */
inline constexpr unsigned int BLOCK_SERIALIZED_SIZE_LIMIT = 1000000;
/** Absolute BIP141 block weight ceiling. */
inline constexpr unsigned int BLOCK_WEIGHT_LIMIT = 1000000;
/** Absolute block sigops cost ceiling (weight / 50). */
inline constexpr int64_t BLOCK_SIGOPS_COST_LIMIT = 20000;

static_assert(BLOCK_SERIALIZED_SIZE_LIMIT == 1000000, "Serialized block size locked to 1 MiB");
static_assert(BLOCK_WEIGHT_LIMIT == 1000000, "Block weight locked to 1_000_000");
static_assert(BLOCK_SIGOPS_COST_LIMIT == 20000, "Block sigops cost locked to 20_000");
static_assert(BLOCK_SIGOPS_COST_LIMIT * 50 == static_cast<int64_t>(BLOCK_WEIGHT_LIMIT),
    "Sigops cost must equal weight/50");

// ---------------------------------------------------------------------------
// RFC-000 §20.1: Chain Identity Lock
// ---------------------------------------------------------------------------

/** chain_name MUST be: "RIGOR" (RFC-000 §20.1). This value is informational only. */
inline constexpr const char* CHAIN_NAME = "RIGOR";
static_assert(std::string_view(CHAIN_NAME) == "RIGOR", "Chain name locked to RIGOR per RFC-000 §20.1");

// ---------------------------------------------------------------------------
// RFC-000 §20.2: Bech32 HRP Lock
// ---------------------------------------------------------------------------

/** bech32_hrp MUST be: "rg" (RFC-000 §20.2, lowercase per Bech32 standard). This applies to address encoding only. */
inline constexpr const char* BECH32_HRP = "rg";
static_assert(std::string_view(BECH32_HRP) == "rg", "Bech32 HRP locked to rg per RFC-000 §20.2");

// ---------------------------------------------------------------------------
// RFC-000 §17: REJECT CODES
// ---------------------------------------------------------------------------

/** RFC-000 §17 reject codes - MUST use these exact strings for consensus failures. */
namespace RejectCodes {
    inline constexpr const char* BAD_ENCODING = "bad-encoding";
    inline constexpr const char* BAD_HASH_LENGTH = "bad-hash-length";
    inline constexpr const char* BAD_WEIGHT = "bad-weight";
    inline constexpr const char* BAD_HEIGHT = "bad-height";
    inline constexpr const char* BAD_SELECTION = "bad-selection";
    inline constexpr const char* BAD_ROOT = "bad-root";
    inline constexpr const char* BAD_BOUNDARY = "bad-boundary";
}

static_assert(COIN == 100000000, "ORU base unit must match Bitcoin-style COIN (1e8)");

// ---------------------------------------------------------------------------
// RES amount scale + supply ceiling (single name: RES_MAX_SUPPLY)
// ---------------------------------------------------------------------------

/** Smallest RES consensus increment: 1 atom == 0.01 RES. */
inline constexpr CAmount RES_ATOMIC_UNIT{1};
/** RES atoms per 1.00 RES (two decimal places). */
inline constexpr CAmount RES_COIN{100};

/** Maximum RES atoms mintable via the consensus RES draw path (hard cap). */
inline constexpr CAmount RES_MAX_SUPPLY = static_cast<CAmount>(21000000LL) * RES_COIN;

static_assert(RES_ATOMIC_UNIT == 1, "RES atomic unit locked");
static_assert(RES_COIN == 100, "RES display scale locked");
static_assert(RES_MAX_SUPPLY == static_cast<CAmount>(2100000000LL), "RES max supply atoms locked");

// ---------------------------------------------------------------------------
// Convergence / RES draw (see convergenceres.cpp / convergence_logic / ConnectBlock)
// ---------------------------------------------------------------------------

/** Minimum ORU convergenced per valid convergence transaction (structure + draw). */
inline constexpr CAmount MIN_CONVERGENCE_AMOUNT = COIN;

/** Minimum count of convergence txs in a block before an RES mint may be considered. */
inline constexpr int CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1;

/**
 * SECURITY-001-A9-C: width, in blocks, of the sliding window the draw's candidate set F is
 * drawn from. The draw resolved at height h enumerates F over blocks [h-W, h-1] rather than
 * over block h-1 alone.
 *
 * WHY THIS EXISTS. Under W = 1 the producer of block h-1 chose F unilaterally and at zero
 * marginal cost: excluding a competitor's convergence tx is free, and no consensus rule can
 * compel inclusion (a node cannot prove what was in a mempool). With
 * CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1, a single self-owned tx was a valid draw, so a miner
 * could win with the MINIMUM 1 ORU burn in every block he produced. RES allocation therefore
 * trended to hashrate rather than to burn, contradicting the rule that PoW does not directly
 * generate RES. See docs/security/TASK-SECURITY-001-A8-DESIGN-REVIEW.md finding F-1.
 *
 * WHAT W FIXES. Widening the window does NOT reduce the fraction of F a large miner controls
 * (a miner with hashrate α still produces ≈ αW of the W blocks). It removes EXCLUSIVITY: he can
 * no longer construct a draw in which he is the only candidate, because the candidates in
 * [h-W, h-2] are already buried and unremovable. He must therefore outbid candidates he cannot
 * censor, which re-couples RES acquisition to burn rather than to hashrate.
 *
 * DERIVATION. The binding constraint is that no single producer can supply the entire window.
 * For hashrate share α that probability is α^W. Design maximum α = 0.5 with a target below
 * 1e-9 gives W ≥ 30; 32 is the next power of two, so RES_PROBABILITY_TARGET is a single shift
 * of the pre-windowing target and log2(W) is an integer.
 *
 *   α = 0.25 → 5.4e-20      α = 0.33 → 3.9e-16      α = 0.50 → 2.3e-10
 *
 * A-8 IS NOT WEAKENED. Every σ input is still fixed the instant block h-1 is found: txid_i was
 * mined earlier, C spans blocks all of which are then fixed, and B_{h-1} is the PoW solution.
 * Re-rolling still costs one block subsidy per sample, and a re-mine can now vary only the
 * source block's own contribution to F.
 */
inline constexpr int CONVERGENCE_DRAW_WINDOW = 32;

/** log2(CONVERGENCE_DRAW_WINDOW); RES_PROBABILITY_TARGET is T >> this. */
inline constexpr int CONVERGENCE_DRAW_WINDOW_LOG2 = 5;

static_assert(CONVERGENCE_DRAW_WINDOW == (1 << CONVERGENCE_DRAW_WINDOW_LOG2),
    "Draw window must be a power of two so T/W is an exact shift");
static_assert(CONVERGENCE_DRAW_WINDOW == 32, "RES convergence draw window locked to 32 blocks");

/**
 * Nominal public-network block spacing (seconds) used in convergence/RES cadence docs.
 * Not every chain uses this for Params::nPowTargetSpacing (e.g. devtest tuning).
 */
inline constexpr int TARGET_BLOCK_TIME = 600;

/** RES draw mint amount when a block awards RES (100.00 RES). */
inline constexpr CAmount RES_REWARD_AMOUNT = 100 * RES_COIN;

static_assert(MIN_CONVERGENCE_AMOUNT == COIN, "Convergence minimum locked to 1 ORU");
static_assert(CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1, "RES convergence draw minimum cardinality locked to 1");
static_assert(RES_REWARD_AMOUNT == 100 * RES_COIN, "RES mint reward locked to 100.00 RES");
static_assert(RES_REWARD_AMOUNT > 0, "RES reward must be positive");
static_assert(RES_MAX_SUPPLY >= RES_REWARD_AMOUNT, "RES cap must admit at least one draw mint");
static_assert(RES_MAX_SUPPLY % RES_REWARD_AMOUNT == 0,
    "RES_MAX_SUPPLY must divide evenly by RES_REWARD_AMOUNT (no stranded tail / mint deadlock)");

// ---------------------------------------------------------------------------
// ORU block subsidy (Rigor networks; GetBlockSubsidy + chainparams)
// ---------------------------------------------------------------------------

/** Initial subsidy base before halving shifts (1000 ORU). */
inline constexpr CAmount ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN;

/** Standard halving interval on Rigor public networks using ORU_SUBSIDY_HALVING_INTERVAL. */
inline constexpr int ORU_SUBSIDY_HALVING_INTERVAL = 210000;

/** Maximum halving shifts (subsidy floor at 125 ORU). */
inline constexpr int ORU_SUBSIDY_MAX_SHIFTS = 3;

/** Subsidy after ORU_SUBSIDY_MAX_SHIFTS right-shifts of ORU_INITIAL_BLOCK_SUBSIDY. */
inline constexpr CAmount ORU_MIN_BLOCK_SUBSIDY = (ORU_INITIAL_BLOCK_SUBSIDY >> ORU_SUBSIDY_MAX_SHIFTS);

static_assert(ORU_MIN_BLOCK_SUBSIDY == 125 * COIN, "ORU post-halving floor locked");

// ---------------------------------------------------------------------------
// Convergence draw weight f_i (RFC-000 §3, §7): f MUST be integer, ≥ 1, NOT transformed.
// The β = 7/5 exponent weighting has been REMOVED per RFC-000 consensus constitution.
// The only selection rule is `H_i < T · f_i` with f_i taken directly from the
// on-chain quantity (Mint Cost for RES mint, transaction fee for RES spend).
// No exponent, no scaling, no normalization, no floating point.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// OpuHash PoW (canonical parameters; implementation in pow_opus.cpp)
// ---------------------------------------------------------------------------

inline constexpr std::size_t OPUS_POW_BUFFER_SIZE = 64 * 1024;
inline constexpr int OPUS_POW_MIX_ROUNDS = 256;

static_assert(OPUS_POW_BUFFER_SIZE == 65536, "OpuHash buffer size locked");
static_assert(OPUS_POW_MIX_ROUNDS == 256, "OpuHash mix rounds locked");

// ---------------------------------------------------------------------------
// ASERT (mainnet τ; testnets override Params::nPowASERTHalfLife)
// ---------------------------------------------------------------------------

inline constexpr int64_t MAINNET_POW_ASERT_HALF_LIFE = 172800;

static_assert(MAINNET_POW_ASERT_HALF_LIFE == 172800, "Mainnet ASERT half-life locked");

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RIGOR_CONSENSUS_LOCK_H
