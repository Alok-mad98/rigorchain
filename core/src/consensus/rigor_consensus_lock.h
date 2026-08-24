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

/** Smallest RES consensus increment: 1 atom == 0.000001 RES. */
inline constexpr CAmount RES_ATOMIC_UNIT{1};
/** RES atoms per 1.000000 RES (six decimal places). */
inline constexpr CAmount RES_COIN{1000000};

/** Maximum RES atoms mintable via the consensus RES draw path (hard cap). */
inline constexpr CAmount RES_MAX_SUPPLY = static_cast<CAmount>(21000000LL) * RES_COIN;

static_assert(RES_ATOMIC_UNIT == 1, "RES atomic unit locked");
static_assert(RES_COIN == 1000000, "RES display scale locked");
static_assert(RES_MAX_SUPPLY == static_cast<CAmount>(21000000000000LL), "RES max supply atoms locked");

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

// ---------------------------------------------------------------------------
// RES mint reward schedule (RFC-017)
//
// The reward decays on a height schedule while the draw target T stays constant. The ORU cost
// of one mint is therefore the protocol constant C = 2^(256-E), and the cost floor per RES is
// C / W(k): a constant divided by a protocol function, with no dependence on demand, on the ORU
// subsidy, or on any chain state. See docs/rfc/RFC-017-SCHEDULED-RES-REWARD.md.
//
// TERMINATION IS EXPLICIT. `W(k) := 0 for k > RES_REWARD_LAST_EPOCH` is a stated rule, not a
// consequence of the reward flooring away at the atomic unit. Under the implicit form the
// horizon was an artifact of RES_COIN — raising the display precision would have silently
// extended issuance, overflowed the isqrt intermediate, and breached the cap. With the cutoff
// stated, the admissible genesis reward is scale-invariant:
//
//   W0 * SUM_{k=0..250} (k+1)^(-3/2) <= RES_MAX_SUPPLY / RES_REWARD_EPOCH = 100 * RES_COIN
//   SUM = 2.486262  =>  W0 <= 40.221 RES, independent of RES_COIN.
// ---------------------------------------------------------------------------

/** W(0): reward for the first epoch (40.000000 RES). */
inline constexpr CAmount RES_REWARD_GENESIS = 40 * RES_COIN;

/** Reward epoch length in blocks; bridged to ORU_SUBSIDY_HALVING_INTERVAL below. */
inline constexpr int RES_REWARD_EPOCH = 210000;

/** Last epoch paying a non-zero reward. Issuance stops at the start of the next one. */
inline constexpr int RES_REWARD_LAST_EPOCH = 250;

/**
 * Exact integer square root, floor(sqrt(n)), by Newton's method on uint64. constexpr so the cap
 * identity below folds at compile time. The largest argument the schedule produces is
 * (251^3 << 32) = 6.79e16, a factor of ~272 below 2^64.
 */
inline constexpr uint64_t ResIntegerSqrt(uint64_t n)
{
    if (n < 2) return n;
    uint64_t x = n;
    uint64_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/**
 * W(k) = floor(RES_REWARD_GENESIS * (k+1)^(-3/2)), evaluated in fixed point at 2^-16:
 * isqrt(x << 32) == floor(sqrt(x) * 2^16), so the two shifts cancel through the division.
 * Integer only — no floating point and no fixed-point pow — hence bit-identical on every
 * platform. Division floors, so W_int(k) <= W_real(k) pointwise and the cap bound below is
 * one-sided in the safe direction.
 */
inline constexpr CAmount ResRewardForEpoch(int k)
{
    if (k < 0 || k > RES_REWARD_LAST_EPOCH) return 0;
    const uint64_t e = static_cast<uint64_t>(k) + 1;
    const uint64_t scale = ResIntegerSqrt((e * e * e) << 32);
    return static_cast<CAmount>((static_cast<uint64_t>(RES_REWARD_GENESIS) << 16) / scale);
}

/** Consensus RES mint reward for a block at `height`. Zero once the schedule is exhausted. */
inline constexpr CAmount GetResMintReward(int height)
{
    if (height < 0) return 0;
    return ResRewardForEpoch(height / RES_REWARD_EPOCH);
}

/** Sum of the whole reward schedule in atoms; finite by construction (251 terms). */
inline constexpr CAmount ResScheduleSum()
{
    CAmount sum = 0;
    for (int k = 0; k <= RES_REWARD_LAST_EPOCH; ++k) sum += ResRewardForEpoch(k);
    return sum;
}

static_assert(MIN_CONVERGENCE_AMOUNT == COIN, "Convergence minimum locked to 1 ORU");
static_assert(CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1, "RES convergence draw minimum cardinality locked to 1");
static_assert(RES_REWARD_GENESIS == 40 * RES_COIN, "RES genesis mint reward locked to 40.000000 RES");
static_assert(RES_REWARD_LAST_EPOCH == 250, "RES reward schedule length locked");
static_assert(ResRewardForEpoch(0) == RES_REWARD_GENESIS, "W(0) must reproduce the genesis reward");
static_assert(ResRewardForEpoch(RES_REWARD_LAST_EPOCH) > 0, "the last scheduled epoch must still pay");
static_assert(ResRewardForEpoch(RES_REWARD_LAST_EPOCH + 1) == 0, "issuance must stop after the last epoch");
static_assert(GetResMintReward(0) == RES_REWARD_GENESIS, "height 0 sits in epoch 0");
static_assert(GetResMintReward(RES_REWARD_EPOCH) < RES_REWARD_GENESIS, "reward must decay across an epoch boundary");
static_assert(RES_MAX_SUPPLY >= RES_REWARD_GENESIS, "RES cap must admit at least one draw mint");
static_assert(RES_MAX_SUPPLY % RES_REWARD_EPOCH == 0,
    "cap must divide evenly by the epoch length for the bound below to be exact");

/**
 * THE CAP IDENTITY (RFC-017 §4). At most one mint may occur per block and each epoch is exactly
 * RES_REWARD_EPOCH blocks, so maximum conceivable issuance is RES_REWARD_EPOCH * ResScheduleSum()
 * — with no assumption whatsoever about demand, burn rate, or participant behaviour.
 *
 * This replaces the former `RES_MAX_SUPPLY % RES_REWARD_AMOUNT == 0` assertion, which was a
 * statement about a fixed reward and is meaningless once the reward varies. The cap thereby
 * stops being an inductive invariant maintained by a runtime check and becomes a compile-time
 * arithmetic fact: no reachable chain, honest or adversarial, can approach RES_MAX_SUPPLY.
 *
 * ConnectBlock does not re-check the cap at runtime. ObservationalMetrics ledgers are
 * post-validation index/explorer state only.
 */
static_assert(ResScheduleSum() <= RES_MAX_SUPPLY / RES_REWARD_EPOCH,
    "reward schedule cannot exhaust RES_MAX_SUPPLY even if every block from genesis mints");

/** Pins the folded value so any drift in the schedule is a compile error, not a silent change. */
static_assert(ResScheduleSum() == 99450437,
    "RES reward schedule sum changed; regenerate golden vectors and the Coq cap lemma");

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
static_assert(RES_REWARD_EPOCH == ORU_SUBSIDY_HALVING_INTERVAL,
    "RES reward epoch and ORU halving interval share one cadence");

// ---------------------------------------------------------------------------
// RES-transfer ORU fee burn (RFC-018)
//
// RES spends already pay network fees on the ORU leg (POOL-F / nFeesRes == 0).
// A fixed share of those fees is permanently unclaimable by coinbase, so ORU
// acquires a sink tied to RES circulation rather than to RES minting / Convergence.
// Pure ORU txs, Convergence, and ORU-funded RES mints are unaffected.
//
// Burn is implemented by lowering the ConnectBlock / CreateNewBlock claimable
// ORU cap; the unclaimed fee never enters the UTXO set (no OP_RETURN parking).
// ---------------------------------------------------------------------------

/** Numerator of the RES-transfer fee burn share (floor(fee * NUM / DEN)). */
inline constexpr int ORU_RES_TRANSFER_FEE_BURN_NUM = 1;
/** Denominator of the RES-transfer fee burn share. */
inline constexpr int ORU_RES_TRANSFER_FEE_BURN_DEN = 2;

static_assert(ORU_RES_TRANSFER_FEE_BURN_NUM > 0, "burn share numerator must be positive");
static_assert(ORU_RES_TRANSFER_FEE_BURN_DEN > 0, "burn share denominator must be positive");
static_assert(ORU_RES_TRANSFER_FEE_BURN_NUM <= ORU_RES_TRANSFER_FEE_BURN_DEN,
    "burn share must not exceed 100%");

/** Floor share of an ORU fee that coinbase must not claim for a RES-transfer tx. */
[[nodiscard]] inline constexpr CAmount OruResTransferFeeBurnShare(CAmount fee)
{
    if (fee <= 0) {
        return 0;
    }
    return (fee * static_cast<CAmount>(ORU_RES_TRANSFER_FEE_BURN_NUM)) /
           static_cast<CAmount>(ORU_RES_TRANSFER_FEE_BURN_DEN);
}

static_assert(OruResTransferFeeBurnShare(0) == 0, "zero fee burns nothing");
static_assert(OruResTransferFeeBurnShare(1) == 0, "odd-satoshi floor burns nothing at 1/2");
static_assert(OruResTransferFeeBurnShare(2) == 1, "1/2 share burns one satoshi of a two-satoshi fee");
static_assert(OruResTransferFeeBurnShare(125 * COIN) == (125 * COIN) / 2,
    "half of one post-halving subsidy block of fees is claimable as burn example");

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
//
// τ = 12 hours. Parent-time evaluation means a miner cannot ease their own
// block; the next block can move by at most 2^(MAX_FUTURE_BLOCK_TIME / τ) ≈
// 1.122×, which sits inside the ±12.5% per-block clamp below.
// Height 1 (first mined block) is the time/target anchor once it exists, so
// launch delay versus the frozen genesis wall-clock cannot collapse difficulty.
// ---------------------------------------------------------------------------

inline constexpr int64_t MAINNET_POW_ASERT_HALF_LIFE = 43200;

/** First mined height: ASERT time/target anchor once `pindexLast->nHeight >= 1`. */
inline constexpr int ASERT_ANCHOR_HEIGHT = 1;

/** Per-block target ratio unit (parts-per-million). Shared by ASERT output clamp and headers-sync. */
inline constexpr int64_t ASERT_RATIO_UNIT_PPM = 1'000'000;
/** Floor of next/prev target ratio (−12.5%). */
inline constexpr int64_t MIN_ADJUSTMENT_FACTOR_PPM = 875'000;
/** Ceiling of next/prev target ratio (+12.5%). */
inline constexpr int64_t MAX_ADJUSTMENT_FACTOR_PPM = 1'125'000;

static_assert(MAINNET_POW_ASERT_HALF_LIFE == 43200, "Mainnet ASERT half-life locked");
static_assert(ASERT_ANCHOR_HEIGHT == 1, "ASERT anchor height locked");
static_assert(MIN_ADJUSTMENT_FACTOR_PPM < ASERT_RATIO_UNIT_PPM, "min clamp must be a decrease");
static_assert(MAX_ADJUSTMENT_FACTOR_PPM > ASERT_RATIO_UNIT_PPM, "max clamp must be an increase");
static_assert(MAX_ADJUSTMENT_FACTOR_PPM * 2 > MIN_ADJUSTMENT_FACTOR_PPM, "clamp window must be non-empty");

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RIGOR_CONSENSUS_LOCK_H
