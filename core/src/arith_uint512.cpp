// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint512.h>
#include <uint256.h>
#include <crypto/common.h>

// ---------------------------------------------------------------------------
// base_uint<BITS> template-member definitions for 512-bit instantiation
//
// `arith_uint256.cpp` defines and explicitly instantiates these for BITS=256.
// To produce the corresponding symbols for BITS=512 (needed by
// `arith_uint512` users such as `consensus/selection.cpp`), the bodies must
// be visible in *this* translation unit before the `template class
// base_uint<512>;` instantiation below. Definitions are intentionally kept
// in lock-step with `arith_uint256.cpp` — keep them synchronized if either
// is modified.
// ---------------------------------------------------------------------------

// Note: `base_uint<BITS>::base_uint(const std::string&)` and the GetHex/
// SetHex/ToString family are intentionally *not* defined for BITS=512 in
// this TU. They would require `base_blob<512>` instantiations which the
// project does not provide, and no consumer of `arith_uint512` needs hex
// I/O — only arithmetic and comparisons (used by RFC-008 §7 cross-
// multiplication in `Consensus::SelectWinner` and `Consensus::IsEligible`).
// Calls to the string ctor / SetHex on arith_uint512 will produce a linker
// error by design; if such a use is later required, also instantiate
// `base_blob<512>` in `uint256.cpp`.

template <unsigned int BITS>
base_uint<BITS>& base_uint<BITS>::operator<<=(unsigned int shift)
{
    base_uint<BITS> a(*this);
    for (int i = 0; i < WIDTH; i++)
        pn[i] = 0;
    int k = shift / 32;
    shift = shift % 32;
    for (int i = 0; i < WIDTH; i++) {
        if (i + k + 1 < WIDTH && shift != 0)
            pn[i + k + 1] |= (a.pn[i] >> (32 - shift));
        if (i + k < WIDTH)
            pn[i + k] |= (a.pn[i] << shift);
    }
    return *this;
}

template <unsigned int BITS>
base_uint<BITS>& base_uint<BITS>::operator>>=(unsigned int shift)
{
    base_uint<BITS> a(*this);
    for (int i = 0; i < WIDTH; i++)
        pn[i] = 0;
    int k = shift / 32;
    shift = shift % 32;
    for (int i = 0; i < WIDTH; i++) {
        if (i - k - 1 >= 0 && shift != 0)
            pn[i - k - 1] |= (a.pn[i] << (32 - shift));
        if (i - k >= 0)
            pn[i - k] |= (a.pn[i] >> shift);
    }
    return *this;
}

template <unsigned int BITS>
base_uint<BITS>& base_uint<BITS>::operator*=(uint32_t b32)
{
    uint64_t carry = 0;
    for (int i = 0; i < WIDTH; i++) {
        uint64_t n = carry + (uint64_t)b32 * pn[i];
        pn[i] = n & 0xffffffff;
        carry = n >> 32;
    }
    return *this;
}

template <unsigned int BITS>
base_uint<BITS>& base_uint<BITS>::operator*=(const base_uint& b)
{
    base_uint<BITS> a;
    for (int j = 0; j < WIDTH; j++) {
        uint64_t carry = 0;
        for (int i = 0; i + j < WIDTH; i++) {
            uint64_t n = carry + a.pn[i + j] + (uint64_t)pn[j] * b.pn[i];
            a.pn[i + j] = n & 0xffffffff;
            carry = n >> 32;
        }
    }
    *this = a;
    return *this;
}

template <unsigned int BITS>
base_uint<BITS>& base_uint<BITS>::operator/=(const base_uint& b)
{
    base_uint<BITS> div = b;
    base_uint<BITS> num = *this;
    *this = 0;
    int num_bits = num.bits();
    int div_bits = div.bits();
    if (div_bits == 0)
        throw uint_error("Division by zero");
    if (div_bits > num_bits)
        return *this;
    int shift = num_bits - div_bits;
    div <<= shift;
    while (shift >= 0) {
        if (num >= div) {
            num -= div;
            pn[shift / 32] |= (1U << (shift & 31));
        }
        div >>= 1;
        shift--;
    }
    return *this;
}

template <unsigned int BITS>
int base_uint<BITS>::CompareTo(const base_uint<BITS>& b) const
{
    for (int i = WIDTH - 1; i >= 0; i--) {
        if (pn[i] < b.pn[i])
            return -1;
        if (pn[i] > b.pn[i])
            return 1;
    }
    return 0;
}

template <unsigned int BITS>
bool base_uint<BITS>::EqualTo(uint64_t b) const
{
    for (int i = WIDTH - 1; i >= 2; i--) {
        if (pn[i])
            return false;
    }
    if (pn[1] != (b >> 32))
        return false;
    if (pn[0] != (b & 0xfffffffful))
        return false;
    return true;
}

template <unsigned int BITS>
double base_uint<BITS>::getdouble() const
{
    double ret = 0.0;
    double fact = 1.0;
    for (int i = 0; i < WIDTH; i++) {
        ret += fact * pn[i];
        fact *= 4294967296.0;
    }
    return ret;
}

// GetHex / SetHex / ToString omitted: see header note above. They depend on
// base_blob<512> which the project does not instantiate.

template <unsigned int BITS>
unsigned int base_uint<BITS>::bits() const
{
    for (int pos = WIDTH - 1; pos >= 0; pos--) {
        if (pn[pos]) {
            for (int nbits = 31; nbits > 0; nbits--) {
                if (pn[pos] & 1U << nbits)
                    return 32 * pos + nbits + 1;
            }
            return 32 * pos + 1;
        }
    }
    return 0;
}

// Explicit instantiation of only the members needed by `arith_uint512`
// consumers. We intentionally avoid `template class base_uint<512>;` because
// that would force-instantiate GetHex/SetHex/ToString/string-ctor which
// depend on `base_blob<512>` (not provided by this codebase).
template base_uint<512>& base_uint<512>::operator<<=(unsigned int);
template base_uint<512>& base_uint<512>::operator>>=(unsigned int);
template base_uint<512>& base_uint<512>::operator*=(uint32_t);
template base_uint<512>& base_uint<512>::operator*=(const base_uint<512>&);
template base_uint<512>& base_uint<512>::operator/=(const base_uint<512>&);
template int  base_uint<512>::CompareTo(const base_uint<512>&) const;
template bool base_uint<512>::EqualTo(uint64_t) const;
template double base_uint<512>::getdouble() const;
template unsigned int base_uint<512>::bits() const;

// Conversion from uint256 to arith_uint512
arith_uint512 arith_uint512::from_uint256(const uint256& a)
{
    arith_uint512 result;
    // Copy the first 8 32-bit words (256 bits) from uint256
    const unsigned char* bytes = a.begin();
    for (int i = 0; i < 8; i++) {
        result.pn[i] = (uint32_t)bytes[i*4] |
                      ((uint32_t)bytes[i*4+1] << 8) |
                      ((uint32_t)bytes[i*4+2] << 16) |
                      ((uint32_t)bytes[i*4+3] << 24);
    }
    // Upper 256 bits remain zero
    return result;
}

// Conversion from arith_uint512 to uint256 (truncates if necessary)
uint256 arith_uint512::to_uint256() const
{
    unsigned char bytes[32];
    // Copy the first 8 32-bit words (lower 256 bits)
    for (int i = 0; i < 8; i++) {
        bytes[i*4] = pn[i] & 0xff;
        bytes[i*4+1] = (pn[i] >> 8) & 0xff;
        bytes[i*4+2] = (pn[i] >> 16) & 0xff;
        bytes[i*4+3] = (pn[i] >> 24) & 0xff;
    }
    return uint256(bytes);
}
