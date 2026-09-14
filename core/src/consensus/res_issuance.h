// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RES_ISSUANCE_H
#define BITCOIN_CONSENSUS_RES_ISSUANCE_H

#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>

#include <cstdint>
#include <limits>

/**
 * RES issuance under RFC-020: a continuous destroy cost and no mint horizon.
 *
 * Consensus never evaluates the display destroy cost. It evaluates one inequality:
 *
 *     R > 0  →  R · (RES_DESTROY_POLE + h)^2 ≤ B << RES_DESTROY_SHIFT
 *     R = 0  →  B unconstrained
 *
 * Numeric SSOT: tools/res_continuous_price_schedule.py. Everything here is a
 * pure function of (height, amounts). Nothing reads chainstate, the UTXO set,
 * the clock or any global.
 */
namespace Consensus {

// ---------------------------------------------------------------------------
// Parameters (RFC-020 §1)
// ---------------------------------------------------------------------------

/** P0: ORU atoms to mint one whole RES at height 0. Unchanged from RFC-019. */
inline constexpr CAmount RES_DESTROY_COST_GENESIS = 1024 * COIN;

static_assert(RES_COIN == COIN,
    "RFC-020 puts RES on the same eight-decimal scale as ORU");
static_assert(RES_DESTROY_COST_GENESIS == 102400000000LL,
    "genesis destroy cost locked to 1024 ORU");
static_assert(RES_DESTROY_SHIFT == 50 - 10,
    "RES_DESTROY_SHIFT is forced by P0 = 2^10 ORU, not chosen");

// ---------------------------------------------------------------------------
// Display destroy cost (RFC-020 §2). Not a consensus input.
// ---------------------------------------------------------------------------

/**
 * Floor-divided display of destroy_cost(h) = P0 · (H + h)^2 / 2^50.
 *
 * Consensus evaluates CheckResMintArithmetic, which is the same relation with
 * the division cleared. The displayed destroy cost can sit one atom below the
 * exact destroy that mints one whole RES (predicate_matches_destroy_cost).
 * Do not feed this value back into the predicate.
 */
[[nodiscard]] inline constexpr CAmount ResDestroyCostAtHeight(int height)
{
    if (height < 0) return 0;
    const unsigned __int128 pole_h =
        static_cast<unsigned __int128>(RES_DESTROY_POLE) +
        static_cast<unsigned __int128>(static_cast<uint64_t>(height));
    const unsigned __int128 numer =
        static_cast<unsigned __int128>(RES_DESTROY_COST_GENESIS) * pole_h * pole_h;
    return static_cast<CAmount>(numer >> 50);
}

/**
 * Display helper: destroy cost at the first height of observational epoch `k`.
 * Not a consensus input. Treats `k` as a calendar index, not a mint argument.
 */
[[nodiscard]] inline constexpr CAmount ResDestroyCostForEpoch(int k)
{
    if (k < 0) return 0;
    if (k > std::numeric_limits<int>::max() / RES_REWARD_EPOCH) {
        return ResDestroyCostAtHeight(std::numeric_limits<int>::max());
    }
    return ResDestroyCostAtHeight(k * RES_REWARD_EPOCH);
}

/**
 * RFC-020 deletes the mint horizon. Height ≥ 0 is authorized; height < 0 is
 * not. Kept so validation / wallet / RPC call sites compile until they drop
 * the name. Do not re-introduce a last-epoch cut here.
 */
[[nodiscard]] inline constexpr bool ResMintAuthorizedAtHeight(int height)
{
    return height >= 0;
}

// ---------------------------------------------------------------------------
// Derived cap (RFC-020 §4)
// ---------------------------------------------------------------------------

/** Closed-form cap: (S / P0) · H = 2^22 RES. */
[[nodiscard]] inline constexpr CAmount ResDerivedMaxSupply()
{
    return (static_cast<CAmount>(1) << 22) * RES_COIN;
}

/**
 * Maximum RES mintable through `height` under ρ = 100%.
 * cap · h / (h + H), the exact antiderivative of the per-block ceiling.
 */
[[nodiscard]] inline constexpr CAmount ResCumulativeMaxAtHeight(int height)
{
    if (height <= 0) return 0;
    const unsigned __int128 cap =
        static_cast<unsigned __int128>(ResDerivedMaxSupply());
    const unsigned __int128 h = static_cast<unsigned __int128>(height);
    return static_cast<CAmount>((cap * h) / (h + RES_DESTROY_POLE));
}

// ---------------------------------------------------------------------------
// The consensus predicate (RFC-020 §2)
// ---------------------------------------------------------------------------

/**
 * Whether `res_atoms` may be minted at `height` against `burn_atoms` of ORU
 * destroyed by the same transaction.
 *
 *     R · (2^25 + h)^2 ≤ B << 40
 *
 * No division, no floor, no square root, no epoch, no horizon. A plain burn
 * that mints nothing stays legal at any height.
 */
[[nodiscard]] inline constexpr bool CheckResMintArithmetic(CAmount res_atoms,
                                                           CAmount burn_atoms,
                                                           int height)
{
    if (res_atoms < 0 || burn_atoms < 0 || height < 0) return false;
    if (res_atoms == 0) return true;
    if (res_atoms > ResDerivedMaxSupply()) return false;
    if (burn_atoms > MAX_MONEY) return false;

    const unsigned __int128 pole_h =
        static_cast<unsigned __int128>(RES_DESTROY_POLE) +
        static_cast<unsigned __int128>(static_cast<uint64_t>(height));
    const unsigned __int128 required =
        static_cast<unsigned __int128>(res_atoms) * pole_h * pole_h;
    const unsigned __int128 supplied =
        static_cast<unsigned __int128>(burn_atoms) << RES_DESTROY_SHIFT;
    return required <= supplied;
}

/**
 * Wallet / constructor inverse of CheckResMintArithmetic: the largest R that
 * the inequality admits for this burn at this height.
 *
 * Consensus itself never divides. Zero means "mints nothing", not "free".
 */
[[nodiscard]] inline constexpr CAmount ResMintAtomsFromBurn(CAmount burn_atoms, int height)
{
    if (burn_atoms <= 0 || height < 0) return 0;
    const unsigned __int128 pole_h =
        static_cast<unsigned __int128>(RES_DESTROY_POLE) +
        static_cast<unsigned __int128>(static_cast<uint64_t>(height));
    const unsigned __int128 denom = pole_h * pole_h;
    const unsigned __int128 r =
        (static_cast<unsigned __int128>(burn_atoms) << RES_DESTROY_SHIFT) / denom;
    if (r > static_cast<unsigned __int128>(ResDerivedMaxSupply())) {
        return ResDerivedMaxSupply();
    }
    return static_cast<CAmount>(r);
}

/**
 * Wallet / UI inverse of the R = 1 case: ceil((H + h)^2 / 2^40).
 * Consensus never consults this helper.
 */
[[nodiscard]] inline constexpr CAmount ResMinMintBurnAtoms(int height)
{
    if (height < 0) return 0;
    const unsigned __int128 pole_h =
        static_cast<unsigned __int128>(RES_DESTROY_POLE) +
        static_cast<unsigned __int128>(static_cast<uint64_t>(height));
    const unsigned __int128 pole2 = pole_h * pole_h;
    const unsigned __int128 den = static_cast<unsigned __int128>(1) << RES_DESTROY_SHIFT;
    return static_cast<CAmount>((pole2 + den - 1) / den);
}

/**
 * Smallest burn that mints `res_atoms` at `height`.
 * ceil(R · (H + h)^2 / 2^40). Wallet-side; consensus never divides.
 */
[[nodiscard]] inline constexpr CAmount ResMinBurnForAtoms(CAmount res_atoms, int height)
{
    if (res_atoms <= 0 || height < 0) return 0;
    const unsigned __int128 pole_h =
        static_cast<unsigned __int128>(RES_DESTROY_POLE) +
        static_cast<unsigned __int128>(static_cast<uint64_t>(height));
    const unsigned __int128 numer =
        static_cast<unsigned __int128>(res_atoms) * pole_h * pole_h;
    const unsigned __int128 den = static_cast<unsigned __int128>(1) << RES_DESTROY_SHIFT;
    const unsigned __int128 need = (numer + den - 1) / den;
    if (need > static_cast<unsigned __int128>(MAX_MONEY)) {
        return MAX_MONEY + 1;
    }
    return static_cast<CAmount>(need);
}

// ---------------------------------------------------------------------------
// Compile-time facts (RFC-020 §2, §4)
// ---------------------------------------------------------------------------

static_assert(ResDestroyCostAtHeight(0) == RES_DESTROY_COST_GENESIS,
    "destroy_cost(0) must reproduce the genesis destroy cost exactly, with no rounding");
static_assert(ResDestroyCostAtHeight(1) == 102400006103LL,
    "destroy_cost(1) drifted; regenerate against tools/res_continuous_price_schedule.py");
static_assert(ResDestroyCostAtHeight(1) > ResDestroyCostAtHeight(0),
    "the displayed destroy cost must harden from height 0 to 1");
static_assert(ResDestroyCostAtHeight(RES_DESTROY_POLE) == 409600000000LL,
    "destroy_cost(H) is 4× genesis; the pole is the hardening timescale");
static_assert(ResDestroyCostAtHeight(-1) == 0,
    "negative heights yield the non-cost sentinel");

static_assert(ResMintAuthorizedAtHeight(0), "genesis height may mint");
static_assert(ResMintAuthorizedAtHeight(std::numeric_limits<int>::max()),
    "there is no horizon; INT32_MAX remains authorized");
static_assert(!ResMintAuthorizedAtHeight(-1), "negative heights are not authorized");

static_assert(!CheckResMintArithmetic(1, 0, 0),
    "a zero burn must never mint a RES atom");
static_assert(!CheckResMintArithmetic(1, 0, std::numeric_limits<int>::max()),
    "INT32_MAX is not a free mint");
static_assert(CheckResMintArithmetic(RES_COIN, RES_DESTROY_COST_GENESIS, 0),
    "1024 ORU buys exactly one RES at height 0");
static_assert(!CheckResMintArithmetic(RES_COIN, RES_DESTROY_COST_GENESIS - 1, 0),
    "one atom short of the genesis destroy cost buys nothing");
static_assert(CheckResMintArithmetic(0, MAX_MONEY, 0),
    "a plain burn that mints nothing is legal");
static_assert(!CheckResMintArithmetic(-1, MAX_MONEY, 0),
    "negative mints are rejected rather than wrapping");
static_assert(!CheckResMintArithmetic(RES_COIN, RES_DESTROY_COST_GENESIS, -1),
    "negative heights cannot mint");

static_assert(ResDerivedMaxSupply() == 419430400000000LL,
    "derived RES cap changed; regenerate golden vectors and the Coq cap theorem");
static_assert(RES_MAX_SUPPLY == ResDerivedMaxSupply(),
    "RES_MAX_SUPPLY must be the closed-form cap, not a declared 21e6");
static_assert(ResDerivedMaxSupply() < MAX_MONEY,
    "the derived cap must stay inside the CAmount sanity bound");
static_assert(ResCumulativeMaxAtHeight(1) < ResDerivedMaxSupply(),
    "the prefix at height 1 is strictly below the cap");
static_assert(ResCumulativeMaxAtHeight(0) == 0, "nothing is mintable at genesis");

static_assert(ResMintAtomsFromBurn(RES_DESTROY_COST_GENESIS, 0) == RES_COIN,
    "the wallet inverse must reproduce one RES at the genesis destroy cost");
static_assert(CheckResMintArithmetic(
                  ResMintAtomsFromBurn(RES_DESTROY_COST_GENESIS, 0),
                  RES_DESTROY_COST_GENESIS, 0),
    "the maximised mint must be admissible");
static_assert(!CheckResMintArithmetic(
                  ResMintAtomsFromBurn(RES_DESTROY_COST_GENESIS, 0) + 1,
                  RES_DESTROY_COST_GENESIS, 0),
    "one atom past the inverse must be underpaid");
static_assert(ResMintAtomsFromBurn(RES_DESTROY_COST_GENESIS - 1, 0) ==
                  RES_COIN - 1,
    "one ORU atom short of the destroy cost buys one RES atom short of a whole RES");
static_assert(ResMintAtomsFromBurn(0, 0) == 0, "a zero burn mints nothing");
static_assert(ResMintAtomsFromBurn(MAX_MONEY, -1) == 0,
    "a negative height must not invent a free mint");

static_assert(ResMinMintBurnAtoms(0) == 1024,
    "height 0: 1024 ORU atoms buy one RES atom");
static_assert(ResMinMintBurnAtoms(1) == 1025,
    "height 1: the one-atom floor steps to 1025");
static_assert(CheckResMintArithmetic(1, ResMinMintBurnAtoms(0), 0),
    "the one-atom mint floor must be admissible");
static_assert(!CheckResMintArithmetic(1, ResMinMintBurnAtoms(0) - 1, 0),
    "one atom short of the floor mints nothing");
static_assert(ResMintAtomsFromBurn(ResMinMintBurnAtoms(0), 0) == 1,
    "the floor must buy exactly one RES atom");
static_assert(ResMinMintBurnAtoms(-1) == 0,
    "a negative height has no mint floor");

static_assert(CheckResMintArithmetic(
                  RES_COIN, ResMinBurnForAtoms(RES_COIN, 1), 1),
    "exact ceil burn at height 1 must buy one RES");
static_assert(!CheckResMintArithmetic(
                  RES_COIN, ResMinBurnForAtoms(RES_COIN, 1) - 1, 1),
    "one atom short of the height-1 ceil is underpaid");

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RES_ISSUANCE_H
