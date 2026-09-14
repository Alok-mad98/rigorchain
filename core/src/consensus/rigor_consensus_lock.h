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
inline constexpr int RIGOR_CONSENSUS_LOCK_VERSION = 3;
static_assert(RIGOR_CONSENSUS_LOCK_VERSION == 3, "Rigor consensus lock version mismatch");

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
/** Per-output / per-tx-sum sanity bound (amount.h MAX_MONEY). Not an ORU supply cap. */
static_assert(MAX_MONEY == static_cast<CAmount>(10000000000LL) * COIN,
    "MAX_MONEY must stay the 10-billion-ORU overflow bound, not Bitcoin 21e6");

// ---------------------------------------------------------------------------
// RES amount scale + supply ceiling (single name: RES_MAX_SUPPLY)
// ---------------------------------------------------------------------------

/** Smallest RES consensus increment: 1 atom == 0.00000001 RES. */
inline constexpr CAmount RES_ATOMIC_UNIT{1};
/** RES atoms per 1.00000000 RES. Same eight-decimal scale as ORU COIN. */
inline constexpr CAmount RES_COIN{100000000};

/** Derived RES cap (RFC-020): (S / P0) · H = 2^7 / 2^10 · 2^25 = 2^22 RES. */
inline constexpr CAmount RES_MAX_SUPPLY = 419430400000000LL;

static_assert(RES_ATOMIC_UNIT == 1, "RES atomic unit locked");
static_assert(RES_COIN == 100000000, "RES display scale locked to 1e8");
static_assert(RES_COIN == COIN, "RES and ORU share the eight-decimal atom");
static_assert(RES_MAX_SUPPLY == 419430400000000LL, "RES max supply is the RFC-020 closed-form cap");
static_assert(RES_MAX_SUPPLY == (1LL << 22) * RES_COIN, "cap must be 2^22 RES atoms");

// ---------------------------------------------------------------------------
// Convergence burn (see convergenceres.h; the OP_CONVERGE burn survives RFC-019)
// ---------------------------------------------------------------------------

/** Smallest positive ORU burn admitted on an OP_CONVERGE output (one atom).
 *
 *  This is not a mint price. Whether a burn mints RES is decided only by
 *  CheckResMintArithmetic. The 1 ORU lottery-ticket floor is retired: the
 *  R = 1 case of the mint inequality is B ≥ ceil((2^25+h)^2 / 2^40), which
 *  is 1024 atoms at height 0. Do not re-introduce a second consensus minimum. */
inline constexpr CAmount MIN_CONVERGENCE_AMOUNT = 1;


/**
 * Nominal public-network block spacing (seconds) used in convergence/RES cadence docs.
 * Not every chain uses this for Params::nPowTargetSpacing (e.g. devtest tuning).
 */
inline constexpr int TARGET_BLOCK_TIME = 600;

// ---------------------------------------------------------------------------
// RFC-020 destroy-cost pole + observational epoch calendar
//
// Consensus minting is R · (RES_DESTROY_POLE + h)^2 ≤ B << RES_DESTROY_SHIFT.
// There is no issuance horizon and no epoch in that predicate.
//
// RES_REWARD_EPOCH is an observational calendar (RPC / ComputeRigorIssuance)
// only. It is not a mint argument.
// ---------------------------------------------------------------------------

/** Hardening timescale H = 2^25 blocks. destroy_cost(h) = P0 · (H + h)^2 / 2^50. */
inline constexpr uint64_t RES_DESTROY_POLE = 1ULL << 25;

/** Forced by P0 = 1024 ORU: 50 − log2(1024) = 40. Not an independent choice. */
inline constexpr int RES_DESTROY_SHIFT = 40;

/** Observational calendar length in blocks; not a mint argument. */
inline constexpr int RES_REWARD_EPOCH = 210000;

static_assert(MIN_CONVERGENCE_AMOUNT == 1, "Convergence minimum locked to one ORU atom");
static_assert(RES_DESTROY_POLE == 33554432ULL, "RES destroy pole locked to 2^25");
static_assert(RES_DESTROY_SHIFT == 40, "RES destroy shift locked; forced by genesis destroy cost");
static_assert(RES_REWARD_EPOCH == 210000, "observational epoch length locked");

// ---------------------------------------------------------------------------
// ORU block subsidy (RFC-020: flat 128 ORU for every height >= 1)
//
// GetBlockSubsidy reads ORU_FLAT_BLOCK_SUBSIDY only. FrozenKernelParams.cpp_base
// must equal this value.
// ---------------------------------------------------------------------------

/** Per-block subsidy for every height >= 1. */
inline constexpr CAmount ORU_FLAT_BLOCK_SUBSIDY = 128 * COIN;

/** Height-0 coinbase / GetBlockSubsidy(0). Not a premine; ConnectBlock still skips genesis UTXO. */
inline constexpr CAmount ORU_GENESIS_BLOCK_SUBSIDY = 0;

static_assert(ORU_GENESIS_BLOCK_SUBSIDY == 0, "genesis must issue 0 ORU");
static_assert(ORU_FLAT_BLOCK_SUBSIDY == 128 * COIN, "h>=1 ORU subsidy is flat 128");
static_assert(ORU_FLAT_BLOCK_SUBSIDY == 12800000000LL,
    "FrozenKernelParams.cpp_base in ConsensusKernelRefinement.v");

// ---------------------------------------------------------------------------
// RES-transfer ORU fee burn (RFC-018)
//
// RES spends already pay network fees on the ORU leg (POOL-F / nFeesRes == 0).
// A fixed share of those fees is permanently unclaimable by coinbase, so ORU
// acquires a sink tied to RES circulation rather than to RES minting / Convergence.
// Pure ORU txs, Convergence, and ORU-funded RES mints are unaffected.
//
// Burn is implemented by lowering the ConnectBlock / CreateNewBlock claimable
// ORU cap; the unclaimed fee never enters the UTXO set (no RIGOR_MARKER_RETURN parking).
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
static_assert(OruResTransferFeeBurnShare(ORU_FLAT_BLOCK_SUBSIDY) == ORU_FLAT_BLOCK_SUBSIDY / 2,
    "half of one flat-subsidy block of fees is claimable as burn example");

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
