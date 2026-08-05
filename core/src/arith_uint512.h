// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ARITH_UINT512_H
#define BITCOIN_ARITH_UINT512_H

#include <arith_uint_base.h>
#include <uint256.h>

/** 512-bit unsigned big integer. */
class arith_uint512 : public base_uint<512> {
public:
    arith_uint512() {}
    arith_uint512(const base_uint<512>& b) : base_uint<512>(b) {}
    arith_uint512(uint64_t b) : base_uint<512>(b) {}
    explicit arith_uint512(const std::string& str) : base_uint<512>(str) {}

    // Conversion from uint256 (truncates or extends)
    static arith_uint512 from_uint256(const uint256& a);
    // Conversion to uint256 (truncates if necessary)
    uint256 to_uint256() const;
};

#endif // BITCOIN_ARITH_UINT512_H
