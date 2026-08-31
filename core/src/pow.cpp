// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <consensus/rigor_consensus_lock.h>
#include <primitives/block.h>
#include <rigor_kernel/pow_digest.h>
#include <sync.h>
#include <uint256.h>
#include <util/hasher.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <unordered_map>

static_assert(Consensus::MAINNET_POW_ASERT_HALF_LIFE >= 4 * MAX_FUTURE_BLOCK_TIME,
              "mainnet ASERT half-life must keep max-future next-block ease inside a small factor");

namespace {

using Consensus::ASERT_RATIO_UNIT_PPM;
using Consensus::MAX_ADJUSTMENT_FACTOR_PPM;
using Consensus::MIN_ADJUSTMENT_FACTOR_PPM;

Mutex g_pow_ok_mutex;
std::unordered_map<uint256, uint32_t, BlockHasher> g_pow_ok GUARDED_BY(g_pow_ok_mutex);
std::deque<uint256> g_pow_ok_fifo GUARDED_BY(g_pow_ok_mutex);
constexpr size_t POW_OK_CACHE_MAX = 8192;

/** Q16.16 radix for aserti3-2d exponent (65536). */
constexpr int64_t ASERT_RADIX = 65536;

/** aserti3-2d cubic coefficients (Bitcoin ABC pow/aserti32d.cpp; BCH Nov 2020 spec). */
constexpr int64_t ASERT_POLY_P1 = 195766423245049LL;
constexpr int64_t ASERT_POLY_P2 = 971821376LL;
constexpr int64_t ASERT_POLY_P3 = 5127LL;
constexpr int64_t ASERT_POLY_BIAS = (1LL << 47);

/** Limit |exponent_q| so shift/mul stay representable. */
constexpr int64_t ASERT_EXP_CLAMP = ASERT_RADIX * 64;

/** 320-bit LE limbs: uint256 * uint64 without truncation. */
using DaaWide = std::array<uint64_t, 5>;

void MulU256ByU64Wide(const arith_uint256& a_in, uint64_t b, DaaWide& out)
{
    out.fill(0);
    arith_uint256 a = a_in;
    for (size_t i = 0; i < 4; ++i) {
        const uint64_t limb = a.GetLow64();
        a >>= 64;
        const unsigned __int128 prod = static_cast<unsigned __int128>(limb) * static_cast<unsigned __int128>(b);
        uint64_t p_lo = static_cast<uint64_t>(prod);
        uint64_t p_hi = static_cast<uint64_t>(prod >> 64);
        unsigned __int128 acc = static_cast<unsigned __int128>(out[i]) + static_cast<unsigned __int128>(p_lo);
        out[i] = static_cast<uint64_t>(acc);
        uint64_t carry = static_cast<uint64_t>(acc >> 64) + p_hi;
        for (size_t k = i + 1; k < 5 && carry != 0; ++k) {
            unsigned __int128 s = static_cast<unsigned __int128>(out[k]) + static_cast<unsigned __int128>(carry);
            out[k] = static_cast<uint64_t>(s);
            carry = static_cast<uint64_t>(s >> 64);
        }
    }
}

arith_uint256 WideDivU64(DaaWide w, uint64_t den)
{
    assert(den > 0);
    uint64_t r = 0;
    for (int i = 4; i >= 0; --i) {
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(r) << 64) | static_cast<unsigned __int128>(w[i]);
        w[i] = static_cast<uint64_t>(t / den);
        r = static_cast<uint64_t>(t % den);
    }
    arith_uint256 ret{0};
    for (size_t i = 0; i < 5; ++i) {
        arith_uint256 term;
        term = w[i];
        ret += term << (int)(64 * i);
    }
    return ret;
}

arith_uint256 MulDivU256(const arith_uint256& value, uint64_t num, uint64_t den)
{
    DaaWide wide;
    MulU256ByU64Wide(value, num, wide);
    return WideDivU64(wide, den);
}

/** 384-bit LE limbs for (target * factor) << num_shifts before the final >> 16. */
using AsertWide = std::array<uint64_t, 6>;

void AsertWideFromMul256x64(AsertWide& v, const arith_uint256& a, uint64_t b)
{
    DaaWide w;
    MulU256ByU64Wide(a, b, w);
    v.fill(0);
    for (size_t i = 0; i < 5; ++i) {
        v[i] = w[i];
    }
}

void AsertWideShl1(AsertWide& v, bool& wide_overflow)
{
    wide_overflow = wide_overflow || ((v[5] & (1ULL << 63)) != 0);
    for (int i = 5; i > 0; --i) {
        v[static_cast<size_t>(i)] = (v[static_cast<size_t>(i)] << 1) | (v[static_cast<size_t>(i - 1)] >> 63);
    }
    v[0] <<= 1;
}

void AsertWideShr1(AsertWide& v)
{
    for (size_t i = 0; i < 5; ++i) {
        v[i] = (v[i] >> 1) | (v[i + 1] << 63);
    }
    v[5] >>= 1;
}

arith_uint256 AsertWideToTarget(const AsertWide& v, const arith_uint256& pow_limit)
{
    if (v[4] != 0 || v[5] != 0) {
        return pow_limit;
    }
    arith_uint256 next_target{0};
    for (size_t i = 0; i < 4; ++i) {
        const arith_uint256 term{v[i]};
        next_target += term << (int)(64 * i);
    }
    if (next_target > pow_limit) {
        return pow_limit;
    }
    return next_target;
}

arith_uint256 AsertNextTargetWide(const arith_uint256& target_ref, uint64_t factor, int64_t num_shifts,
                                  const arith_uint256& pow_limit)
{
    AsertWide v;
    AsertWideFromMul256x64(v, target_ref, factor);
    bool wide_overflow = false;

    if (num_shifts < 0) {
        const unsigned shr = static_cast<unsigned>(std::min<int64_t>(-num_shifts, 64));
        for (unsigned i = 0; i < shr; ++i) {
            AsertWideShr1(v);
        }
    } else if (num_shifts > 0) {
        const unsigned shl = static_cast<unsigned>(std::min<int64_t>(num_shifts, 64));
        for (unsigned i = 0; i < shl; ++i) {
            AsertWideShl1(v, wide_overflow);
        }
    }

    for (unsigned i = 0; i < 16; ++i) {
        AsertWideShr1(v);
    }

    if (wide_overflow) {
        return pow_limit;
    }
    return AsertWideToTarget(v, pow_limit);
}

const CBlockIndex* AsertAnchorIndex(const CBlockIndex* pindexLast)
{
    assert(pindexLast != nullptr);
    if (pindexLast->nHeight < Consensus::ASERT_ANCHOR_HEIGHT) {
        return pindexLast;
    }
    // Fast path: well-formed contiguous index (production / IBD). GetAncestor
    // asserts pprev on a skip miss, so refuse it on synthetic/fuzz chains whose
    // height does not match pprev->nHeight + 1.
    if (pindexLast->pprev != nullptr &&
        pindexLast->pprev->nHeight == pindexLast->nHeight - 1) {
        const CBlockIndex* ancestor = pindexLast->GetAncestor(Consensus::ASERT_ANCHOR_HEIGHT);
        if (ancestor != nullptr && ancestor->nHeight == Consensus::ASERT_ANCHOR_HEIGHT) {
            return ancestor;
        }
    }
    const CBlockIndex* walk = pindexLast;
    while (walk->pprev != nullptr && walk->nHeight > Consensus::ASERT_ANCHOR_HEIGHT &&
           walk->pprev->nHeight < walk->nHeight) {
        walk = walk->pprev;
        if (walk->nHeight == Consensus::ASERT_ANCHOR_HEIGHT) {
            return walk;
        }
    }
    return walk;
}

arith_uint256 ClampTargetToPrev(const arith_uint256& next_target, uint32_t prev_nbits,
                                const arith_uint256& pow_limit)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 prev_target;
    prev_target.SetCompact(prev_nbits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || prev_target == 0) {
        return next_target;
    }

    arith_uint256 max_target = MulDivU256(prev_target, static_cast<uint64_t>(MAX_ADJUSTMENT_FACTOR_PPM),
                                          static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    arith_uint256 min_target = MulDivU256(prev_target, static_cast<uint64_t>(MIN_ADJUSTMENT_FACTOR_PPM),
                                          static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    if (max_target > pow_limit) {
        max_target = pow_limit;
    }
    if (min_target == 0) {
        min_target = 1;
    }
    if (min_target > pow_limit) {
        min_target = pow_limit;
    }

    arith_uint256 clamped = next_target;
    if (clamped > max_target) {
        clamped = max_target;
    }
    if (clamped < min_target) {
        clamped = min_target;
    }
    if (clamped > pow_limit) {
        clamped = pow_limit;
    }
    if (clamped == 0) {
        clamped = 1;
    }
    return clamped;
}

/**
 * ASERT (aserti3-2d), BCH Nov 2020 integer path.
 *
 * Anchor: genesis while producing height 1; height 1 thereafter (TASK-012 R4-B).
 * Evaluation time/height: the parent header (TASK-012 R1). The child nTime does
 * not enter the exponent, so GBT ntime rolling cannot produce bad-diffbits.
 *
 * time_error = (parent_time - anchor_time) - (parent_height - anchor_height) * spacing
 * exponent_q = trunc( time_error * 65536 / half_life )
 * next = clamp_to_prev(anchor_target * 2^(exponent_q / 65536))
 */
unsigned int GetNextWorkASERT(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    (void)pblock;
    if (params.nPowASERTHalfLife <= 0) {
        return pindexLast->nBits;
    }

    const CBlockIndex* const pindexAnchor = AsertAnchorIndex(pindexLast);

    const int64_t height_diff =
        static_cast<int64_t>(pindexLast->nHeight) - static_cast<int64_t>(pindexAnchor->nHeight);
    const int64_t time_error =
        static_cast<int64_t>(pindexLast->GetBlockTime()) - static_cast<int64_t>(pindexAnchor->GetBlockTime()) -
        height_diff * params.nPowTargetSpacing;

    const __int128 numer = static_cast<__int128>(time_error) * static_cast<__int128>(ASERT_RADIX);
    const __int128 denom = static_cast<__int128>(params.nPowASERTHalfLife);
    int64_t exp_q = static_cast<int64_t>(numer / denom);

    if (exp_q > ASERT_EXP_CLAMP) {
        exp_q = ASERT_EXP_CLAMP;
    } else if (exp_q < -ASERT_EXP_CLAMP) {
        exp_q = -ASERT_EXP_CLAMP;
    }

    const int64_t num_shifts = exp_q >> 16;
    const int64_t exp_rem = exp_q - (num_shifts << 16);

    const __int128 e = static_cast<__int128>(exp_rem);
    const __int128 poly =
        static_cast<__int128>(ASERT_POLY_P1) * e +
        static_cast<__int128>(ASERT_POLY_P2) * e * e +
        static_cast<__int128>(ASERT_POLY_P3) * e * e * e +
        static_cast<__int128>(ASERT_POLY_BIAS);
    const uint64_t factor = static_cast<uint64_t>(poly >> 48) + static_cast<uint64_t>(ASERT_RADIX);

    const arith_uint256 pow_limit = UintToArith256(params.powLimit);

    arith_uint256 target_ref;
    target_ref.SetCompact(pindexAnchor->nBits);

    arith_uint256 next_target = AsertNextTargetWide(target_ref, factor, num_shifts, pow_limit);

    if (next_target == 0) {
        next_target = 1;
    }
    if (next_target > pow_limit) {
        next_target = pow_limit;
    }

    next_target = ClampTargetToPrev(next_target, pindexLast->nBits, pow_limit);

    return next_target.GetCompact();
}

} // namespace

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    const unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }

    if (params.fPowAllowMinDifficultyBlocks && pblock != nullptr) {
        if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2) {
            return nProofOfWorkLimit;
        }
    }

    return GetNextWorkASERT(pindexLast, pblock, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }

    // Legacy Bitcoin-style single-span retarget (used by tests / tooling). Kept for compatibility.
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4) {
        nActualTimespan = params.nPowTargetTimespan / 4;
    }
    if (nActualTimespan > params.nPowTargetTimespan * 4) {
        nActualTimespan = params.nPowTargetTimespan * 4;
    }

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }

    return bnNew.GetCompact();
}

bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks) {
        return true;
    }

    if (height <= 0) {
        return true;
    }

    bool fNegative_old, fOverflow_old, fNegative_new, fOverflow_new;
    arith_uint256 old_target;
    old_target.SetCompact(old_nbits, &fNegative_old, &fOverflow_old);
    arith_uint256 new_target;
    new_target.SetCompact(new_nbits, &fNegative_new, &fOverflow_new);

    if (fNegative_old || fOverflow_old || old_target == 0 || fNegative_new || fOverflow_new || new_target == 0) {
        return false;
    }

    const arith_uint256 pow_limit = UintToArith256(params.powLimit);
    arith_uint256 max_target = MulDivU256(old_target, static_cast<uint64_t>(MAX_ADJUSTMENT_FACTOR_PPM),
                                           static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    arith_uint256 min_target = MulDivU256(old_target, static_cast<uint64_t>(MIN_ADJUSTMENT_FACTOR_PPM),
                                           static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    if (max_target > pow_limit) {
        max_target = pow_limit;
    }
    if (min_target > pow_limit) {
        min_target = pow_limit;
    }

    arith_uint256 max_enc;
    max_enc.SetCompact(max_target.GetCompact());
    arith_uint256 min_enc;
    min_enc.SetCompact(min_target.GetCompact());

    if (new_target > max_enc) {
        return false;
    }
    if (new_target < min_enc) {
        return false;
    }
    return true;
}

bool CheckProofOfWork(const CBlockHeader& block, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit)) {
        return false;
    }

    const uint256 header_hash = block.GetHash();

    if (header_hash == params.hashGenesisBlock) {
        return true;
    }

    // Success cache: the same header is hashed in CheckHeadersPoW and again in
    // ContextualCheckBlockHeader. Failures are not cached (random junk must not
    // evict honest entries). Keying on the block id stays sound after SPEC-008:
    // the id determines the 80 header bytes, and the digest is a function of
    // exactly those bytes.
    {
        LOCK(g_pow_ok_mutex);
        const auto it = g_pow_ok.find(header_hash);
        if (it != g_pow_ok.end() && it->second == nBits) {
            return true;
        }
    }

    const uint256 pow_hash = rigor_kernel::GetRigorPowHash(block);

    if (UintToArith256(pow_hash) > bnTarget) {
        return false;
    }

    {
        LOCK(g_pow_ok_mutex);
        if (g_pow_ok.size() >= POW_OK_CACHE_MAX) {
            g_pow_ok.erase(g_pow_ok_fifo.front());
            g_pow_ok_fifo.pop_front();
        }
        auto [it, inserted] = g_pow_ok.emplace(header_hash, nBits);
        if (inserted) {
            g_pow_ok_fifo.push_back(header_hash);
        } else {
            it->second = nBits;
        }
    }

    return true;
}
