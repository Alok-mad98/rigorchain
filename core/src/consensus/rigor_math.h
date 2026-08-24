// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Integer-only selection inequality helpers (RFC-000 §3, §13).
// The ONLY selection rule is `H_i < T · f_i`. No exponent, no scaling, no
// normalization, no floating point. 256×256 → 512-bit multiplication is the
// minimum arithmetic width required to compare H against T·f without loss.

#ifndef BITCOIN_CONSENSUS_RIGOR_MATH_H
#define BITCOIN_CONSENSUS_RIGOR_MATH_H

#include <arith_uint256.h>
#include <consensus/rigor_consensus_lock.h>
#include <crypto/common.h>
#include <uint256.h>

#include <cassert>
#include <cstdint>
#include <cstring>

namespace Consensus {

/** 512-bit product container for H vs T·f comparison (256×256 → 512, RFC-000 §13). */
class uint512
{
public:
    uint64_t m_w[8]{};

    uint512() = default;
    explicit uint512(const arith_uint256& x);

    bool IsZero() const;
    int Compare(const uint512& b) const;
    bool operator<(const uint512& b) const { return Compare(b) < 0; }
    bool operator<=(const uint512& b) const { return Compare(b) <= 0; }
    bool operator==(const uint512& b) const { return Compare(b) == 0; }
    bool operator!=(const uint512& b) const { return Compare(b) != 0; }
};

namespace rigor_math_detail {

inline void ArithToLe64(const arith_uint256& x, uint64_t out[4])
{
    arith_uint256 t = x;
    for (int i = 0; i < 4; ++i) {
        out[i] = t.GetLow64();
        t >>= 64;
    }
}

inline uint512 Mul256x256Full(const arith_uint256& a_in, const arith_uint256& b_in)
{
    uint64_t a[4], b[4];
    ArithToLe64(a_in, a);
    ArithToLe64(b_in, b);
    uint64_t p[8]{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const unsigned __int128 prod = static_cast<unsigned __int128>(a[i]) * static_cast<unsigned __int128>(b[j]);
            int k = i + j;
            unsigned __int128 acc = static_cast<unsigned __int128>(p[k]) + static_cast<uint64_t>(prod);
            p[k] = static_cast<uint64_t>(acc);
            unsigned __int128 carry = (prod >> 64) + (acc >> 64);
            while (carry != 0) {
                ++k;
                assert(k < 8);
                acc = static_cast<unsigned __int128>(p[k]) + carry;
                p[k] = static_cast<uint64_t>(acc);
                carry = acc >> 64;
            }
        }
    }
    uint512 out{};
    std::memcpy(out.m_w, p, sizeof(p));
    return out;
}

} // namespace rigor_math_detail

inline uint512::uint512(const arith_uint256& x)
{
    std::memset(m_w, 0, sizeof(m_w));
    uint64_t limbs[4];
    rigor_math_detail::ArithToLe64(x, limbs);
    for (int i = 0; i < 4; ++i) {
        m_w[i] = limbs[i];
    }
}

inline bool uint512::IsZero() const
{
    for (uint64_t limb : m_w) {
        if (limb != 0) return false;
    }
    return true;
}

inline int uint512::Compare(const uint512& o) const
{
    for (int i = 7; i >= 0; --i) {
        if (m_w[i] < o.m_w[i]) return -1;
        if (m_w[i] > o.m_w[i]) return 1;
    }
    return 0;
}

/**
 * RFC-008 §6: Core inequality (the sole consensus rule)
 * 6.1 Definition: H_i < T · f_i
 * 6.2 Evaluation rules: Left side: H_i → uint256, Right side: T · f_i → computed in uint512
 * ❗CRITICAL: Comparison MUST be exact integer comparison, NO division, NO floating point, NO scaling
 *
 * RFC-008 §7: Multiplication rules (Critical)
 * 7.1 MUST use 512-bit: uint512 product = (uint512)T * (uint512)f_i
 * ❗FORBIDDEN: uint256 overflow ❌, modulo arithmetic ❌, truncation ❌
 *
 * RFC-008 §8: Winner Selection (no division)
 * 8.1 Order definition: i ≺ j iff H_i * f_j < H_j * f_i
 * 8.2 Implementation: Left = (uint512)H_i * f_j, Right = (uint512)H_j * f_i
 * ❗FORBIDDEN: H_i / f_i ❌, float comparison ❌
 *
 * RFC-008 §9: Equivalence note (theory vs implementation)
 * Mathematical form (permitted in papers only): argmin(H_i / f_i)
 * Implementation form (the only legal one): H_i * f_j < H_j * f_i
 * ❗RULE: Division form MUST NEVER appear in implementation
 *
 * RFC-008 §1: Mathematical domain definition (Domain Separation)
 * 1.1 The three domains (MUST stay isolated): Arithmetic Domain (Integer arithmetic, Comparisons, Multiplication)
 * Mandatory rule: Arithmetic MUST NEVER mix domains implicitly
 *
 * RFC-008 §2: Basic data types (Canonical Types)
 * 2.1 Definition: uint256 := 256-bit unsigned integer, uint512 := 512-bit unsigned integer (for multiplication only)
 * Forbidden: No floating point, No signed integer, No implicit casting, No big-endian reinterpretation
 *
 * RFC-009 §7: Comparison inputs (Cross Multiplication)
 * Left  = H_i * f_j
 * Right = H_j * f_i
 * ❗Serialization rules
 * - H_i MUST be uint256
 * - f_i MUST be uint64
 * - multiplication MUST promote to uint512
 *
 * RFC-009 §6: f_i encoding
 * f_i MUST be uint64 (little-endian)
 * ❗Definition: Mint: f_i = Mint Cost (base units), Spend: f_i = fee (base units)
 * ❌ FORBIDDEN: float fee, scaled fee, weight normalization
 *
 * RFC-008 §12: Forbidden optimizations (highly critical)
 * ❌ "optimizing" into a float comparison
 * ❌ using double
 * ❌ dividing early
 * ❌ truncating to 256-bit
 * ❌ altering the concatenation order
 * ❌ automatic endian conversion
 * ❌ using a library's default bignum (bit width unverified)
 *
 * RIGOR IMPLEMENTATION BIBLE §9: INEQUALITY
 * ONLY VALID FORM: H_i < T * f_i
 * IMPLEMENTATION FORM: H_i * f_j < H_j * f_i
 * ❌ FORBIDDEN: division, float comparison
 *
 * RFC-000 §3: SINGLE INEQUALITY LAW
 * H_i < T · f_i
 * Implementation: H_i * f_j < H_j * f_i (cross-multiply to avoid division)
 *
 * RIGOR IMPLEMENTATION BIBLE §10: BIG INTEGER
 * All multiplication MUST be arbitrary precision
 * ❌ FORBIDDEN: overflow, fixed-width wraparound
 *
 * RFC-009 §9: Prohibited practices
 * Using big integer library without fixed width ❌
 *
 * RFC-000 §8: INTEGER ARITHMETIC
 * ALL math MUST be integer-only. Multiplication MUST use arbitrary precision.
 *
 *   - H is taken as a 256-bit unsigned integer (the Sigma / draw digest).
 *   - T is the immutable RFC-017 shipping constant RES_PROBABILITY_TARGET = floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW (C=1024 ORU)
 *     (carried in `Consensus::Params`). It is NOT derived from `nBits`,
 *     ASERT state, or any header field; `target_prev` is retained in
 *     `bind_commit` for Sigma-CR-01 replay-domain separation only.
 *   - f is the untransformed on-chain integer weight (Mint Cost for a
 *     RES mint candidate; transaction fee for a RES spend candidate). It MUST
 *     be integer and ≥ 1 per RFC-000 §7. No exponent, no normalization.
 *
 * The product T · f is computed at ≥ 512-bit precision (RFC-000 §8) to avoid
 * any silent 256-bit wrap when f is large.
 */
inline bool ConvergenceDrawInequalityHolds(const uint256& h, const uint256& res_probability_target, CAmount f_sats)
{
    const arith_uint256 T = UintToArith256(res_probability_target);
    if (T == 0) return false;
    // RFC-008 §10: Edge Cases
    // 10.1 f_i = 0: MUST reject transaction
    //
    // RFC-009 §6: f_i MUST be uint64 (little-endian)
    // RIGOR IMPLEMENTATION BIBLE §8: WEIGHT
    // f_i MUST be integer, f_i MUST ≥ 1
    // ❌ FORBIDDEN: float, log / sqrt / transform
    if (f_sats < 1 || !MoneyRange(f_sats)) return false;  // RFC-000 §10: IF f_i == 0: REJECT (bad-weight)
    const arith_uint256 f(static_cast<uint64_t>(f_sats));
    // RFC-008 §7: Multiplication rules (Critical)- MUST use 512-bit
    // RFC-008 §8: Winner Selection (no division)- Left = (uint512)H_i * f_j
    // RFC-009 §7: H_i MUST be uint256, multiplication MUST promote to uint512
    const uint512 left(UintToArith256(h));
    // RFC-008 §7: Multiplication rules (Critical)- uint512 product = (uint512)T * (uint512)f_i
    // RFC-008 §8: Winner Selection (no division)- Right = (uint512)H_j * f_i
    // RFC-009 §7: multiplication MUST promote to uint512
    // RIGOR IMPLEMENTATION BIBLE §10: arbitrary precision multiplication (no overflow)
    const uint512 right = rigor_math_detail::Mul256x256Full(T, f);
    // RFC-008 §6.2: Comparison MUST be exact integer comparison
    // RFC-008 §8.2: i ≺ j iff H_i * f_j < H_j * f_i
    return left < right;
}

} // namespace Consensus

#endif
