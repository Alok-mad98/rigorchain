// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <policy/feerate.h>
#include <tinyformat.h>

#include <algorithm>
#include <limits>

CFeeRate::CFeeRate(const CAmount& nFeePaid, uint32_t num_bytes)
{
    const int64_t nSize{num_bytes};

    if (nSize > 0) {
        // `nFeePaid * 1000` overflows int64 above ~9.2e15, which is well inside
        // RIGOR's MAX_MONEY. Widen the product, then saturate: a feerate is a
        // policy/display quantity, so a clamp is the correct answer where the
        // exact value is unrepresentable. Nothing in consensus reads CFeeRate.
        const __int128 per_k{static_cast<__int128>(nFeePaid) * 1000 / nSize};
        constexpr __int128 kMax{std::numeric_limits<int64_t>::max()};
        constexpr __int128 kMin{std::numeric_limits<int64_t>::min()};
        nSatoshisPerK = static_cast<CAmount>(std::clamp(per_k, kMin, kMax));
    } else {
        nSatoshisPerK = 0;
    }
}

CAmount CFeeRate::GetFee(uint32_t num_bytes) const
{
    const int64_t nSize{num_bytes};

    // `nSatoshisPerK * nSize` overflows int64 for large feerates, which RIGOR's
    // MAX_MONEY makes reachable. Widen the product and take an exact integer
    // ceiling: truncating division already rounds a negative quotient up, so only
    // a positive remainder needs the extra unit. This also drops the double that
    // the rest of the codebase forbids.
    const __int128 product{static_cast<__int128>(nSatoshisPerK) * nSize};
    __int128 quotient{product / 1000};
    if (product % 1000 > 0) ++quotient;
    constexpr __int128 kMax{std::numeric_limits<int64_t>::max()};
    constexpr __int128 kMin{std::numeric_limits<int64_t>::min()};
    CAmount nFee{static_cast<CAmount>(std::clamp(quotient, kMin, kMax))};

    if (nFee == 0 && nSize != 0) {
        if (nSatoshisPerK > 0) nFee = CAmount(1);
        if (nSatoshisPerK < 0) nFee = CAmount(-1);
    }

    return nFee;
}

std::string CFeeRate::ToString(const FeeEstimateMode& fee_estimate_mode) const
{
    switch (fee_estimate_mode) {
    case FeeEstimateMode::SAT_VB: return strprintf("%d.%03d %s/vB", nSatoshisPerK / 1000, nSatoshisPerK % 1000, CURRENCY_ATOM);
    default:                      return strprintf("%d.%08d %s/kvB", nSatoshisPerK / COIN, nSatoshisPerK % COIN, CURRENCY_UNIT);
    }
}
